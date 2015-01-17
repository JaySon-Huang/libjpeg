libjpeg-9a源码阅读笔记
====
由IJG(Independent JPEG Group)维护的libjpeg库，是常用的操作jpeg文件的C库，但是缺乏文档，难以扩展。  
利用这一个repo做源码阅读以及例程编写，为其他库做基础。

## 安装libjpeg
`Mac OS X`:
(注意至今(2015-01-17)为止,通过`brew`安装的libjpeg仍为8d版本,原因在此[jpeg v9a (Discuss bump) #35371](https://github.com/Homebrew/homebrew/issues/35371))  
    
    brew install libjpeg

## 说明

### jpeg2bmp
利用libjpeg把jpeg格式图像转换为bmp格式图像的例程  

编译:
    
    gcc jpeg2bmp.c -o jpeg2bmp -ljpeg

### jpeggetdct
利用libjpeg提取RGB数据经过dct变换之后的原始数据  

编译:
    
    gcc jpeggetdct.c -o joeggetdct -ljpeg
