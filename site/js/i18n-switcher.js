/* CrossPet Reader — i18n Language Switcher
   Depends on TRANSLATIONS global from i18n-translations.js */

(function () {
  'use strict';

  var DEFAULT_LANG = 'vi';

  function applyLang(lang) {
    if (!TRANSLATIONS[lang]) lang = DEFAULT_LANG;
    document.documentElement.lang = lang;

    document.querySelectorAll('[data-i18n]').forEach(function (el) {
      var key = el.getAttribute('data-i18n');
      var val = TRANSLATIONS[lang][key];
      if (val !== undefined) {
        // Use innerHTML to support inline markup (<strong>, <code>, <em>, <a>)
        el.innerHTML = val;
      }
    });

    document.querySelectorAll('.lang-btn').forEach(function (btn) {
      btn.classList.toggle('active', btn.dataset.lang === lang);
    });

    localStorage.setItem('lang', lang);
  }

  document.addEventListener('DOMContentLoaded', function () {
    var stored = localStorage.getItem('lang') || DEFAULT_LANG;
    applyLang(stored);

    document.querySelectorAll('.lang-btn').forEach(function (btn) {
      btn.addEventListener('click', function () {
        applyLang(btn.dataset.lang);
      });
    });
  });
})();
