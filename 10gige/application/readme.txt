This folder contains the 10gigE application source code

Building Application:
  - Execute 'export CC=aarch64-linux-gnu-gcc' before make to set correct compiler
  - go to Folder Arm64
  - make

Needed files to run application:
  - update_atable
      executable, creates allocation table
      takes app name as argument
      run before first start of gvrd
      creates alloc_table.bin
  - update_eeprom
      executable, updates emulated eeprom
      select option 1 for address and option 1 for dhcp
      run before first start of gvrd and whenever xml changed
      creates eeprom.bin
  - zcip and zcip.script
      needed to get IP when DHCP is not available
      zcip.script must be executable (chmod +x)
  - xgvrd-kr260
      GenICam xml containing device registers and information
  - start.sh
      start script setting shared lib location
      change this to contain current lib path
