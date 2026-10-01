/* Original-roster identity and bounded selection speech adapted from
 * V82VehicleRegistry.cs (6fc8179), Copyright (c) 2026 flaffy, MIT.
 * tools/third_party/LICENSE_RECOMPONE. Portraits use the native VLC layers. */
namespace presentation {
constexpr uint32_t State=0x80330000, Names=State+0x100, Images=0x80340000;
constexpr uint32_t Magic=0x56384944;
std::array<v82::Bytes,17> sections{};
std::array<uint32_t,15> images{};
bool ready=false;
v82::Bytes outcomes{};
uint32_t outcome_lba=0;
void load_outcomes(v82::Bytes bytes) {
    constexpr unsigned header=8+26*8;
    if(bytes.u32(0)!=0x41583856||bytes.u32(4)!=26||bytes.size<header||
       (bytes.size-header)%2336)throw std::runtime_error("invalid outcome bank");
    uint32_t end=0;
    for(unsigned i=0;i<26;++i) {
        const auto start=bytes.u32(8+i*8),count=bytes.u32(12+i*8);
        if(start!=end||!count||count%8||uint64_t(start+uint64_t(count))*2336>bytes.size-header)
            throw std::runtime_error("invalid outcome extent");
        end=start+count;
    }
    if(uint64_t(end)*2336!=bytes.size-header)throw std::runtime_error("outcome trailing data");
    outcomes=bytes;
}
void load(v82::Bytes bytes) {
    ready=false;
    if(bytes.u32(0)!=0x44493856||bytes.u32(4)!=sections.size())throw std::runtime_error("invalid identity header");
    uint32_t end=8+uint32_t(sections.size())*8,guest=Images;
    for(unsigned i=0;i<sections.size();++i) {
        const auto offset=bytes.u32(8+i*8),size=bytes.u32(12+i*8);
        if(offset<end||!size)throw std::runtime_error("overlapping identity records");
        sections[i]=bytes.sub(offset,size);end=offset+size;
        if(i<15) {
            if(size<12||sections[i].u32(4)>>24!=0x38||
               sections[i].u32(8)!=(i<13?0x20004u:0x20008u))throw std::runtime_error("invalid portrait record");
            if((sections[i].u32(4)&0xffff)*4u>0x100000)throw std::runtime_error("portrait scratch overflow");
            images[i]=guest;guest+=(size+3)&~3u;
            if(guest>HeapLo)throw std::runtime_error("portrait storage exceeds enhancement memory");
        }
    }
    if(sections[15].u16(0)!=16||sections[16].u16(0)!=13)throw std::runtime_error("incomplete voice banks");
    ready=true;
}
void init() {
    if(rd(State)==Magic)return;
    if(!ready)fail("v82.presentation.missing_data");
    for(unsigned i=0;i<15;++i)for(uint32_t j=0;j<sections[i].size;++j)
        psx_mod_write_byte(images[i]+j,sections[i].data[j]);
    for(unsigned i=0;i<package.vehicles.size();++i) {
        const auto& name=package.vehicles[i].name;
        if(name.size()>=64)fail("v82.presentation.name_too_long");
        for(unsigned j=0;j<=name.size();++j)psx_mod_write_byte(Names+i*64+j,j<name.size()?uint8_t(name[j]):0);
    }
    wr(State,Magic);
}
// Retail names live in an eighteen-entry table. Imported object types use
// their own immutable strings, including local players whose native name
// buffer is blank because no retail roster entry initialized it.
bool object_name(CPUState* c) {
    const auto type=psx_mod_read_byte(c->gpr[4]+0xDC);
    if(type<64||type-64>=package.vehicles.size())return false;
    init();c->gpr[2]=Names+(type-64)*64;psx_mod_finish_function(c);
    psx_mod_counter_add("v82.presentation.object_name",1);return true;
}
bool result_entry(CPUState* c,uint32_t address) {
    if(address==0x8005FA9C && c->gpr[31]==0x80013268) {
        const auto player=c->gpr[18];if(player>=4)return false;
        const auto type=psx_mod_read_byte(c->gpr[28]+0x1104+player);
        if(type<64||type-64>=package.vehicles.size())return false;
        init();
        std::string path="Shared\\V8VOICE\\";
        path+=psx_mod_read_byte(c->gpr[28]+0x33)?'V':'D';
        const unsigned index=type-64;path+=char('0'+index/10);path+=char('0'+index%10);path+=".XA";
        for(unsigned i=0;i<=path.size();++i)psx_mod_write_byte(c->gpr[4]+i,i<path.size()?uint8_t(path[i]):0);
        wr(State+4,1);c->gpr[2]=uint32_t(path.size());psx_mod_finish_function(c);
        psx_mod_counter_add("v82.presentation.result_path",1);return true;
    }
    if(address==0x8001DA14 && c->gpr[31]==0x80013294 && rd(State+4)) {
        c->gpr[5]=0;wr(State+4,0);psx_mod_counter_add("v82.presentation.result_voice",1);
    }
    if(address==0x80018210) {
        std::string path;
        for(unsigned i=0;i<32;++i) {
            char ch=char(psx_mod_read_byte(c->gpr[4]+i));if(!ch)break;
            if(ch=='/')ch='\\';if(ch>='a'&&ch<='z')ch-=32;path+=ch;
        }
        const std::string prefix="SHARED\\V8VOICE\\";
        if(path.size()!=prefix.size()+6||path.compare(0,prefix.size(),prefix))return false;
        const auto p=prefix.size();const unsigned index=unsigned(path[p+1]-'0')*10+unsigned(path[p+2]-'0');
        if((path[p]!='D'&&path[p]!='V')||index>=13||path.substr(p+3)!=".XA")return false;
        const unsigned slot=index+(path[p]=='V'?13:0);
        const auto descriptor=State+0x20;
        wr(descriptor+12,outcome_lba+outcomes.u32(8+slot*8));
        wr(descriptor+16,outcomes.u32(12+slot*8)*2048);
        c->gpr[2]=descriptor;psx_mod_finish_function(c);
        psx_mod_counter_add("v82.presentation.result_resolved",1);return true;
    }
    return false;
}
void portrait(CPUState* c,int index,bool pose) {
    init();
    if(index==12&&pose)return;
    c->gpr[4]=images[index==12?12:pose?index:index<6?13:14];
    psx_mod_counter_add("v82.selector.portrait",1);
}
bool voice(CPUState* c,unsigned index,uint32_t shell) {
    const auto channel=c->gpr[4],bank=rd(shell+0x16738);
    psx_mod_counter_add("v82.selector.voice_enter",1);
    if(channel<1||channel>2||!bank||psx_mod_read_half(bank)!=16) {
        psx_mod_counter_add("v82.selector.invalid_voice_bank",1);return false;
    }
    const auto& source=sections[16];
    const unsigned sample=14+channel-1,start=source.u16(4+index*4)*8;
    const unsigned end=index<12?source.u16(8+index*4)*8:source.u16(2)*8;
    const unsigned capacity=(psx_mod_read_half(bank+64)-psx_mod_read_half(bank+60))*8;
    const uint32_t entry=bank+4+sample*4,address=psx_mod_read_half(entry)*8u;
    if(end<=start||(end-start)>capacity||address+capacity>0x80000) {
        psx_mod_counter_add("v82.selector.voice_overflow",1);return false;
    }
    const auto data=source.sub(56+start,end-start);
    original_specials::invoke(c,0x8001DE78,channel);
    psx_mod_counter_add("v82.selector.voice_stopped",1);
    const auto transfer=spu_read(0x1F801DA6);
    spu_write(0x1F801DA6,address>>3);
    for(uint32_t i=0;i<data.size;i+=4)spu_dma_write(data.u32(i));
    spu_write(0x1F801DA6,transfer);
    psx_mod_counter_add("v82.selector.voice_uploaded",1);
    psx_mod_write_half(entry+2,source.u16(6+index*4));
    original_specials::invoke(c,0x8001E188,channel,bank,sample);
    psx_mod_counter_add("v82.selector.original_voice",1);
    return true;
}
}
