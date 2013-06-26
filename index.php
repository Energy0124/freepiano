---
---
<?php
$lang = explode(',',$_SERVER['HTTP_ACCEPT_LANGUAGE']);
$lang = strtolower(substr(chop($lang[0]),0,2));

$lang_remap = array(
  "en" => "en",
  "zh" => "cn"
);

if (!is_array($lang, array_keys($lang_remap)))
  $lang = "en";

header('Location: {{ site.baseurl }}/' . $lang_remap[$lang]);
?>
