---
vim: wrap expandtab ft=markdown
layout: docs
title: Display settings
lang: en
---

FreePiano uses notation by default, if you are not suited, you can custom notes display, FreePiano currently offers three kinds of notes display:

1. Roll method, notation indicates.
2. Fixed roll method, notation indicates.
3. Musical alphabet method, note name indicates.

<div class="note question">
<h5>Why there are only sharp notes?</h5>
<p>In MIDI, the note is represented by a number from 0-127, based on figures in FreePiano expressed within the same note, while the melody is just a pitch shift, FreePiano did not distinguish between the size of the song tune, so only can be used consistently sharp symbol to represent notes.</p>
</div>

## Changing the type of note display

To modify how the note is displayed, select `Config -> Options -> GUI`

![GUI Settings]({{ site.baseurl }}/en/docs/img/gui_setting.png)

Change the value of `Note display`


## Change display type of the piano keyboard

For a song which key signature is not 'C', FreePiano can transpose notes for you. You can custom how note is displayed on the piano keyboard, with the option `Transpose on MIDI keyboard` checked, the fingering of original C keys is is displayed.

## Custorm default colors of a key.

FreePiano provides three scheme of color displaying:

1. Classic mode, all keys is displayed as orange.
2. Channel mode, keys controlling different channels have different colors.
3. Velocity mode, notes with different velocity have different colors.

![Color display of channel mode.]({{ site.baseurl }}/en/docs/img/key_color.png)


## Custom animation speed of a button fade

When a key is released, FreePiano show a smooth fade effect, you can custorm the speed that key fades.

## Custom the transparency of the main window.

In Windows Vista or later, transparency of the main windows can also be changed. this option is not saved. so next time you open FreePiano, it will be always opaque.
