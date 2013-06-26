---
vim: wrap expandtab ft=markdown
layout: page
title: 用电脑键盘演奏音乐
categories: updates
lang: cn
---

FreePiano是一款使用电脑键盘演奏音乐的开源软件。

<div class="play_video" id="video1">
<img src="{{ site.baseurl }}img/screenshot.jpg"/>
</div>

最新版下载： [FreePiano 1.8](http://sourceforge.net/projects/freepiano/files/freepiano_1.8.zip)

## FreePiano有什么特点？
1. 完全免费的，您不需要购买就可以使用全部的功能。
2. 直接调用VSTi音源，不需要安装虚拟MIDI设备。
3. 支持多种音频输出方式，包括DirectSound, WASAPI和ASIO。
4. 可以定义键盘上任意一个按键的功能与显示。
5. 多套键盘配置可以在演奏中任意切换。
6. 可将录制的乐曲导出成视频MP4文件。

##2013-06-24  FreePiano 1.8
* 支持原始MIDI消息输出，FreePiano内部的所有消息都不再使用MIDI消息。
* 现在所有的命令都可以正确的支持所有的设置方式，包括同步设置。
* 支持16个输入通道，每个输入通道都可以映射到一个MIDI输出通道上。
* 增加了一个'跟随曲调'的设置，可以控制一个通道是否跟随当前的曲调。
* 增加了'音色组'选项的设置，可以在支持多乐器的音源上选择音色组。
* 优化了按键设置和通道设置界面。
* 优化了主界面的显示，现在FreePiano仅再需要的时候重新绘制屏幕。
* 增加了很多按键预设命令，预设菜单上按类别分组。
* 现在在XP上也能正常的切换语言了。
* 脚本现在支持多国语言了。
* 可以定义按键的颜色和音符的显示方式。还可以预览整个键盘的色彩。
* 增加了'WAV'文件导出功能。
* FreePiano有新版本以后会提示。
* 修正了之前版本在拷贝粘贴键盘配置时会降低一个八度的BUG。

## 2013-05-24 FreePiano 1.7
* 演奏设置界面，可以选择MIDI输出通道和音色（需要支持多音色的音源）。
* 增加了Set1和Set10这两个修饰，可以仅修改某个值的各位和十位。
* 增加了Transpose命令，可以分别设置左右手的音符变化，<br>
  增加了临时左右手升半音的预设。
* 右键修改按键绑定时，强制所有的脚本都设置到当前按键上，<br>
  脚本中的按键不影响绑定结果， 方便拷贝按键配置。
* 脚本中对通道的显示为Ch_0到Ch_16，修改脚本时不易和其他值混淆位置。
* 将增加和减少分组修改为增加，插入和删除分组。
* 可以在界面设置中设置主窗口的透明度。
* 在音频设置中可以更精确的修改播放速度。
* 修正了脚本编辑器中最后一个字符无用的BUG。
* 修正了脚本中使用C5为中央C的BUG，中央C改为C4。
* 修正了录音时分组不是0则无法正确记录分组的BUG。
* 修正了导出MP4时音量控制不起作用的BUG。

## 2013-2-6 FreePiano 1.6
* 新设计的按键设置窗口， 可以快速的编辑单个键的脚本。
* 添加了固定唱名法的显示。
* 添加了键盘动画，可以在界面设置中调节速度。
* 控制器设置添加了'Sync'修饰，可以与其他合用。<br>
  比如SyncPress会有在下一个音符弹奏时才会有Press效， 可以做出同步踏板的效果。
* 修正了键盘脚本输入框有长度限制的BUG。

## 2013-1-28 FreePiano 1.5.2
* 修正了一些设置无法被保存的BUG。
* 键盘脚本不再有长度限制。
* 为控制器设置添加了'Press'方法， 可以临时改变一个控制器的值，<br>
  20毫秒后再恢复原先的值。

## 2013-1-21 FreePiano 1.5.1
* 修正了XP下无法运行的BUG。

## 2013-1-14 FreePiano 1.5
* 同时支持多个MIDI输入设备，并且支持MIDI设备重新映射通道。
* 将MIDI输出移动到音源菜单中。
* 修改了键盘钩挂机制， 在后台运行时取消键盘钩挂， 使一些安全软件不再提示。
* MIDI 输入的力度不再被键盘力度所影响。
* 修正了预设的控制器设置时未清除原来设置的BUG。
* 修正了某些MIDI键盘抬起按键未被正确识别的BUG
* 修正了Keyup绑定无法正常工作的BUG。

## 2012-10-16  FreePiano 1.4.1
* 修复了在使用MIDI键盘时曲调，力度和八度均无效果的BUG。
* 添加了一个选项，可以显示原曲调和变换到C调的钢琴按键。

## 2012-10-8 FreePiano 1.4
* 修正英文歌曲信息界面上的文字错误。
* 修正了配置文件中无法使用负数的BUG。

* 键盘分组增加到最多255组。
* 界面上添加当前分组显示。
* 中文版界面文字全部改为中文。
* DelayKeyup指令在演奏过程中改变会影响正在演奏的音符的延音效果，<br>
  其行为更像延音踏板。
* 允许一个按键绑定多个命令（一键多音功能）。

## 2012-4-25 FreePiano 1.3
* 修正了在自动选择输出设备时输出延时和音量无法正确保存的BUG。
* 修正了MIDI控制器设置无法正确被读取的BUG。

* 添加英文语言支持。
* 添加键盘标签中文的支持。
* 主音量可以调整到200%。
* 修改默认FlashPiano键位显示按键名称。
* 添加了MP4视频文件导出功能。

## 2011-6-26 FreePiano 1.2
* 修正录音时没有记录MIDI输入的BUG。
* 修正某些按键用右键菜单映射时错误的BUG。
* 修正某些VST插件的兼容性问题。
* 扩展了MIDI控制器指令，可以实现在MIDI控制器上的加减和反转操作。
* 扩展了ProgramChange指令，同MIDI控制器指令。
* 音色、控制器（包括踏板）可以随着组被保存。

## 2011-5-26 FreePiano 1.1
* 修正录音时没有记录MIDI输入的BUG。
* 修正某些按键用右键菜单映射时错误的BUG。
* 修正某些VST插件的兼容性问题。
* 扩展了MIDI控制器指令，可以实现在MIDI控制器上的加减和反转操作。
* 扩展了ProgramChange指令，同MIDI控制器指令。
* 音色、控制器（包括踏板）可以随着组被保存。

## 2011-05-19 FreePiano 1.0
* 全新的用户界面
* 支持乐曲录制，播放，并且能够打开经典的lyt文件。
* 加入了一些状态显示，包括踏板状态、曲调、键盘力度和键盘八度的升降。
* 在界面上加入了录制和播放按钮以及当前播放/录制的时间。
* 键盘上可以配置踏板、曲调、力度和八度等常用功能。


<script type="text/javascript" language="javascript">
  var obj = document.getElementById("video1");
  if (obj) {
    obj.innerHTML += '<div class="icon_play_video"></div>';
    obj.onclick = function() {
      var videos = [ "XNDYwMDkxMzY4", "XNDg0MDQxMDAw", "XNDQ2NzQyODky" ] ;
      var url = "http://player.youku.com/embed/" + videos[Math.floor(Math.random()*videos.length)];
      obj.innerHTML = '<iframe height=325 width=640 src="' + url + '" frameborder=0 allowfullscreen></iframe>';
    }
  }
</script>
