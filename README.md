# hisi_temp_monitor
海思 hi3516 芯片温度监控驱动  
当超过设定的温度时(默认105度)，重启系统

控制节点在 /sys/class/temperature/hisi_temp_monitor 目录下  
cat temp  获取当前温度  
echo 90 > uplimit  设置重启温度(非debug模式下最低80度)  
echo 1 > debug  打开温度打印  