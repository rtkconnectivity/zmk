..\prepend_header\prepend_header.exe -t app_data1 -p "build\zephyr\zmk.bin" -m 1 -c sha256 -b 15
1>NUL ren build\zephyr\zmk.tmp build\zephyr\zmk.bin
1>NUL ren build\zephyr\zmk_MP.bin build\zephyr\zmkImage_MP.bin
..\md5\md5.exe "build\zephyr\zmkImage_MP.bin"