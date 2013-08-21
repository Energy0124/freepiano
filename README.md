# [FreePiano](http://freepiano.tiwb.com)

FreePiano is a virtual MIDI keyboard and a piano for Windows.

You can use freepiano to Play piano with computer keyboard or MIDI keyboard with any vst instrument you like, or output through MIDI, or generate any MIDI message with any key on the keyboard. You can also record your song and export a MP4 movie to share with your friends.

![ScreenShot](res/screenshot.jpg)

## Features of FreePiano:

* Completely free, you do not need to purchase to use all the features.
* Using VSTi, you don't need to install a virtual MIDI device.
* Support a variety of audio output, including DirectSound, WASAPI and ASIO.
* You can define any key on the keyboard and display functions.
* Multi sets of keyboard layout can be switched anytime during play.
* Export your song to mp4 directly.


## Change logs
**2013-06-24  FreePiano 1.8**
* Raw MIDI message support, with 'MIDI' command you can send at most 3 hex
  digitals as a midi message. all other script commands are nolonger MIDI
  signals.
* All script commands works correct with all value operators now, including
  'Sync' and 'Press' modifier.
* Supports at most 16 input channels, which are then mapped to 16 MIDI output
  MIDI channels.
* Adds a 'Follow key' option on input channels, which can control a input
  channel not to follow current key signature.
* Adds a 'Bank' option on output channels, which equals midi controller 0.
* Optimized key popup settings dialog and settings dialog.
* Optimized main screen refresh, freepiano uses less GPU now.
* Preset menu now support groups, adds a lot of preset scripts in common use.
* Language can be changed correctly on windows XP.
* Localized script support.
* Keyboard color support, and you can display note names as 'C D E F G A B' now.
* A new 'WAV' file exportor.
* New update notification.
* Fixed a bug that notes will lower an octave when copy group key maps.


**2013-05-24  FreePiano 1.7**
* Play settings page, can change midi output channel and voice.
* New Set1 and Set10 modifier, can change value by mask 1 or 10.
* Adds a transpose command which can transpose notes on each channel, 
  and adds two presets that sharp on left or right hand.
  Force bindings scripts on popup-menu use selected key,
  make it easy to copy key scripts without modify key name.
* Optimized script display, easier to find which param is channel.
* Change 'increase, decrease' group to 'add, insert, delete' group.
* Transparency of main window can be changed in GUI settings.
* Adds playback speed in audio settings page.
* Fixed a bug that keymap editor will eat the last character.
* Fixed a bug that freepiano uses C5 as middle C.
* Fixed a bug that song recorded wrong group when starting group is not 0.
* Fixed a bug that volume is not used when exporting MP4.

**2013-02-06  FreePiano 1.6**
* New popup key modify menu, with quick script edit.
* Adds fixed-doh display mode.
* Adds keyboard animation, can be changed in gui settings.
* Adds a 'Sync' modifier on controller command, combined use with other
  modifiers. for example 'SyncPress' will do a 'Press' control after playing
  next note.
* Fixed a bug that keyboard map script editor still has a length limit.


**2013-01-28  FreePiano 1.5.2**
* Fixed a bug that some configuration can not be saved.
* The length of keyboard map script now has no limit.
* Adds a 'Press' modifier on Controller command, which can temporary change
  controller value then change it back after 20ms.

**2013-01-21  FreePiano 1.5.1**
* Freepiano 1.5 will not running on windows XP, this path just fixed that.

**2013-01-14  FreePiano 1.5**

* Multiply MIDI input device and remap of midi input channel is now supported.
* MIDI output is now as instruments and can be selected in instrument menu.
* Change keyboard hook method to make anti-virus software happy.
* MIDI input velocity no longer adjusted by key velocity option.
* Fixed a bug that preset controllers menu not clear previous key mapping.
* Fixed a bug that noteoff not handled for some midi keyboard.
* Fixed a bug that keyup bind did not work.

**2012-10-16  FreePiano 1.4.1**

* When using MIDI keyboard, you can also use KEY, VELOCITY and OCTSHIFT on the
  main screen.
* Adds a option that can display original key or transcribed key.


**2012-10-08  FreePiano 1.4**

* Fixed language mistake on song info page.
* Fixed a bug that negative number can not be loaded correctly in config files.
* Maximum key groups increased to 255.
* Current key group is now displayed on main interface.
* Localization on main interface.
* 'DelayKeyup' command change during playing will affect notes current playing.
  Acts more like Sustain pedal now.
* Allows more than one command mapped to a single key.


**2012-04-25  FreePiano 1.3**

* Fixed a bug that volume and output buffer size can't be saved.
* Fixed a bug that unable to read midi controller message.
* Adds English language support.
* Adds Chinese key label support.
* Main volume can changed up to 200%.
* Keyboard map for FlashPiano layout is changed to display key names.
* Adds MP4 video file export.


**2011-06-26  FreePiano 1.2**

* Fixed a bug that midi events not recorded.
* Fixed a bug that mapping some controls to some key may not work.
* Fixed a but that program crashes when loading some VST plugin.
* New extension for MIDI controller message,  Add, Sub, Flip can be used in
  MIDI controller message.
* New extension for MIDI program change message, same as controller message.
* Programs, controller values can be save to group settings.


**2011-05-27 FreePiano 1.1**

* Fixed noise may occur when playing.
* Fixed VST plugin path save error.
* Fixed high CPU usage bug when minimized.
* Fixed set channel BUG in the right-click menu.
* Optimized keymap GUI.
* Adds play speed control.
* Adds disable windows-key function.
* Adds setting groups, setting group can be changed anytime.
* Adds copy paste support for keymap.
* Adds disable resize window option.
* Adds drag-drop support for config and song files.


**2011-05-19 FreePiano 1.0**

* New user interface.
* Supports for record and playback, supports reading LYT file format.
* Adds some status display.
* Adds record and playback button at main interface.
