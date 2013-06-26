---
vim: ts=2 sts=2 wrap expandtab
---
function lang_redirect(url) {
  var lang_map = {
    "en" : "en",
    "zh" : "cn",
  }

  var lang
  var language_info = navigator.language ? navigator.language : navigator.userLanguage;
  if (language_info) {
    lang = lang_map[language_info.substr(0, 2)]
  }
  if (lang === undefined) lang = "en"

  var dest = "{{ site.baseurl }}/" + lang + "/" + url

  if (window) {
    if (window.location.replace) {
      window.location.replace(dest)
    } else {
      window.location=dest
    }
  }
}
