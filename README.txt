## Realtek zephyr repo clarification ##
URL: https://github.com/rtkconnectivity
zephyr: branch:realtek-main-v3.5.0
hal_realtek: branch:main
zmk(manifest repo): branch:rtk_zmk

##setup:
1.git clone --branch rtk_zmk https://github.com/rtkconnectivity/zmk.git
2. west init -l zmk/app
3. west update
4. cd zmk/app
5. west build -p -b rtl8762gn_evb -- -DSHIELD=rtk_keyboard