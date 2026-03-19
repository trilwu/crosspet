/* CrossPet Reader — Site JS
   Nav toggle, scroll animations, anchor scroll, Web Serial detection */

(function () {
  'use strict';

  /* ── Mobile Navigation ── */
  const hamburger = document.querySelector('.nav-hamburger');
  const navLinks  = document.querySelector('.nav-links');

  if (hamburger && navLinks) {
    hamburger.addEventListener('click', () => {
      const isOpen = navLinks.classList.toggle('is-open');
      hamburger.setAttribute('aria-expanded', String(isOpen));
    });

    /* Close menu when a link is clicked */
    navLinks.addEventListener('click', (e) => {
      if (e.target.tagName === 'A') navLinks.classList.remove('is-open');
    });
  }

  /* ── Smooth scroll for in-page anchors ── */
  document.querySelectorAll('a[href^="#"]').forEach((anchor) => {
    anchor.addEventListener('click', (e) => {
      const target = document.querySelector(anchor.getAttribute('href'));
      if (!target) return;
      e.preventDefault();
      target.scrollIntoView({ behavior: 'smooth', block: 'start' });
    });
  });

  /* ── Scroll-triggered fade-in (IntersectionObserver) ── */
  const fadeEls = document.querySelectorAll('.fade-in');
  if (fadeEls.length && 'IntersectionObserver' in window) {
    const observer = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            entry.target.classList.add('visible');
            observer.unobserve(entry.target);
          }
        });
      },
      { threshold: 0.12 }
    );
    fadeEls.forEach((el) => observer.observe(el));
  } else {
    /* Fallback: show all immediately */
    fadeEls.forEach((el) => el.classList.add('visible'));
  }

  /* ── Web Serial detection (flash.html) ── */
  const warning = document.querySelector('.browser-warning');
  if (warning) {
    const hasWebSerial = 'serial' in navigator;
    if (!hasWebSerial) warning.classList.add('visible');
  }

  /* ── Mark active nav link ── */
  const currentPage = window.location.pathname.split('/').pop() || 'index.html';
  document.querySelectorAll('.nav-links a').forEach((link) => {
    const href = link.getAttribute('href');
    if (href === currentPage || (currentPage === '' && href === 'index.html')) {
      link.setAttribute('aria-current', 'page');
    }
  });
})();
