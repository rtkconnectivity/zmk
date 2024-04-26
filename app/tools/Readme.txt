## support Independent dfu ##
#operate flow
1. follow current flashmap, download the independent dfu app to appdata6.addr
2. reset IC and pull high P2_5(press the button on the back of the board)
3. enter independent dfu app, open cfudownloadtool
4. run AppData/ZmkApp/run.bat to generate zmk_mpxx.bin。 use mp pack tool to generate offer and payload bindings in directory A
5. select directory A in cfudownloadtool, then download