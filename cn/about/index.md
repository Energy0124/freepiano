---
vim: wrap expandtab ft=markdown
layout: page
title: 关于
categories: about
lang: cn
---

FreePiano 是一款开源的虚拟MIDI键盘软件。

最初开发FreePiano的目的仅仅是为了个人方便，当时能找到的所有的同类软件都需要通过虚拟MIDI设备才可以使用VST音源，并且不能完全利用键盘上的每一个按键。FreePiano是为了改善以上两点才诞生的。

1.0版本推出以后，越来越多的爱好者开始提出自己对软件的建议。是这些建议让FreePiano一点一点完善，最终成为今天的样子。感谢每一位对FreePiano提出建议的朋友。

如果在开始FreePiano之前，我能找到一款类似的开源软件，也许我会在它的基础上开始实现我想要的功能，可是当时没有。 所以FreePiano开源，我希望以后如果有爱好者想要实现新的功能时，他们可以在现在的基础上开发。


## 软件的开发

李佳是FreePiano最早的开发者， 包括目前的版本（1.8）都是他完成的。你可以通过
![Mail to lijia]({{ site.baseurl }}/img/lijia.png)
联系到他。


## 意见建议、BUG和反馈

如果您对于软件有任何建议，或者在使用中遇到了一些问题，欢迎直接邮件联系我（李佳)
，一般用邮件能够得到比较及时的回复。演奏方面的问题您可以和社区爱好者交流，我本人在演奏方面不是很在行。


## 本地化

FreePiano目前支持中文和英文，英文中可能有一些翻译错误或者不符合本地习惯的词汇，请帮我指正。如果您希望完整的本地化FreePiano（软件或者文档），也请联系我。


## 致谢

FreePiano的发展离不开电脑音乐的爱好者，感谢以下网友在软件开发初期给予我的建议和帮助：
多米诺未完成、各种压力猫君、添雨、idper、Listen ……

同时也感谢所有软件的使用者们，有些朋友接触键盘钢琴的演奏已经超过10年。是你们的坚持让我有信心把软件做的更好。


## 开源软件的使用

### mdaPiano
FreePiano的编译版本中包含了mdaPiano这个小巧的音源，它的主页是：<br>
<http://mda.smartelectronix.com>

### freetype
FreePiano 使用 freetype2 绘制字体，它的主页是：<br>
<http://www.freetype.org>

### zlib
FreePiano 的fpm文件中存放的是用zlib压缩过的数据，请查看：<br>
<http://www.zlib.net>

### libpng
FreePiano 中所有的图片资源都是png格式的，使用了libpng读取png图片：<br>
<http://www.libpng.org>

### x264
x264是目前广泛使用的视频压缩方法，FreePiano使用x264来压缩视频数据。<br>
<http://www.videolan.org/developers/x264.html>

### faac
在导出MP4视频文件时，用faac编码音频数据：<br>
<http://sourceforge.net/projects/faac>

### mp4v2
FreePiano 的MP4文件容器使用了mp4v2库。<br>
<http://code.google.com/p/mp4v2>
