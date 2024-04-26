cp ..\..\..\build\zephyr\zmk.bin .
..\..\prepend_header\prepend_header.exe -t secure_app_data -p "zmk.bin" -m 1 -c sha256 -b 15
1>NUL ren zmk.tmp zmkImage.bin
1>NUL ren zmk_MP.bin zmkImage_MP.bin
..\..\md5\md5.exe "zmkImage_MP.bin"