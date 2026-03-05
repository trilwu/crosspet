/* CrossPet Reader — Dark Mode Toggle
   Reads localStorage or prefers-color-scheme, applies on load, binds toggle button */

(function () {
  'use strict';

  var stored = localStorage.getItem('theme');
  var prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches;
  var theme = stored || (prefersDark ? 'dark' : 'light');
  document.documentElement.setAttribute('data-theme', theme);

  function applyTheme(t) {
    document.documentElement.setAttribute('data-theme', t);
    localStorage.setItem('theme', t);
  }

  /* Bind after DOM ready — toggle button injected by HTML */
  document.addEventListener('DOMContentLoaded', function () {
    var toggle = document.querySelector('.dark-mode-toggle');
    if (!toggle) return;
    toggle.addEventListener('click', function () {
      var current = document.documentElement.getAttribute('data-theme');
      applyTheme(current === 'dark' ? 'light' : 'dark');
    });
  });
})();
