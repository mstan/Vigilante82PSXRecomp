#include "mod_test_runtime.h"

int main() {
    enabled = true;
    for (const auto& h : Hooks) wr(h.pc,h.word);
    CPUState cpu{}; cpu.gpr[31]=0x80010000u;
    auto resize = [&](uint32_t p, uint32_t bytes) {
        cpu.gpr[4]=p;cpu.gpr[5]=bytes;cpu.pc=0;
        entry(&cpu,Realloc);
        if(cpu.pc!=cpu.gpr[31]) throw std::runtime_error("missing allocator return");
        return cpu.gpr[2];
    };
    const auto p=allocate(128);
    wr(p,p+24);wr(p+24,0x12345678);
    const auto shrunk=resize(p,32);
    if(shrunk!=p || rd(shrunk)!=shrunk+24 || rd(rd(shrunk))!=0x12345678)
        throw std::runtime_error("shrinking invalidated a retained internal pointer");
    if(resize(p,HeapHi-HeapLo)!=0 || !owned(p) || rd(p+24)!=0x12345678)
        throw std::runtime_error("failed growth damaged the original allocation");
    const auto grown=resize(p,256);
    if(!grown || grown==p || owned(p) || rd(grown+24)!=0x12345678)
        throw std::runtime_error("growth did not preserve payload and retire old block");
    remember(grown,0);wr(grown,VehicleEvent);
    resize(grown,0);
    if(object_entry(grown)!=-1)
        throw std::runtime_error("free retained identity after native callback replacement");
    const auto whole=allocate(HeapHi-HeapLo-8);
    if(whole!=HeapLo+8) throw std::runtime_error("free blocks failed to coalesce");
    release(whole);
    const auto first=allocate(128),second=allocate(96);
    // Native 80014948 walks a temporary allocation list through header -8.
    wr(first-8,second);wr(second-8,0);
    if(!owned(first)||!owned(second))throw std::runtime_error("native list link replaced ownership");
    release(second);release(first);
    const auto reclaimed=allocate(HeapHi-HeapLo-8);
    if(reclaimed!=HeapLo+8)throw std::runtime_error("native temporary list leaked heap");
    release(reclaimed);
    std::cout << "guest heap pointer stability, growth, failure and lifecycle passed\n";
}
