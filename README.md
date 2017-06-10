# arduino_mt7530
试图使用Arduino通过SMI控制MT7530  

## 目前进度
### 已实现功能：
仅仅是可以在串口读取每个端口的连接速度了。。  
### 坑：
有时候启动会不成功。。其他的功能都不能用。。  

## Todo：
1.搞定VLAN功能：vlan相关的几个api都搞了，测试写入读出也都没有问题，但工作不能（也许是我还没搞懂它的设置方式。。。。  

2.实现网页控制：网页做了一半。。打算放在nodemcu上，  

## 硬件改造方法
首先你需要一个MT7530芯片的交换机，并且这个一般都是有引出SMI接口的。。  
然后你需要找到芯片 pin 107和109对地的电阻空位，并且找个4.7k的补上去（如果没有）  
把arduino的引脚（3.3V可以直连，5V自己加保护）连上去并且上拉3.3v  
就基本上搞定了  




## 参考（自行google）
AP-MT7620A+MT7530-V13-GE-SPI-DDR2-4L-2X2N-140421.pdf  
mt7530.c  
Convert your cheap “unmanaged” switch to a VLAN capable layer 2 managed switch for just $2， August 26, 2015 by Tiziano Bacocco  




