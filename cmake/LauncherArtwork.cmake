set(V82_LAUNCHER_BOXART "" CACHE FILEPATH "Existing V82 launcher cover image (optional)")
psxrecomp_launcher_artwork(V82_LAUNCHER_BOXART
    NAME v82-boxart.png
    URL "https://raw.githubusercontent.com/libretro-thumbnails/Sony_-_PlayStation/25c6d021383b6dd4f7c74bad24845b15f6fb7d04/Named_Boxarts/Vigilante%208%20-%202nd%20Offense%20(USA).png"
    SHA256 "6a572b8bd14eb658adf6dc108858bcd4bcb17ad67933e32070c339350642de76"
    OVERRIDE "${V82_LAUNCHER_BOXART}")
