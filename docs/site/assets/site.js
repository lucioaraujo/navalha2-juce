const header = document.querySelector('[data-site-header]');
const toggle = document.querySelector('.nav-toggle');
const nav = document.querySelector('#main-nav');

if (toggle && nav) {
  toggle.addEventListener('click', () => {
    const isOpen = nav.classList.toggle('open');
    toggle.setAttribute('aria-expanded', String(isOpen));
  });

  nav.addEventListener('click', (event) => {
    if (event.target.matches('a')) {
      nav.classList.remove('open');
      toggle.setAttribute('aria-expanded', 'false');
    }
  });
}

const links = [...document.querySelectorAll('.main-nav a[href^="#"]')];
const sections = links
  .map((link) => document.querySelector(link.getAttribute('href')))
  .filter(Boolean);

if ('IntersectionObserver' in window && sections.length) {
  const observer = new IntersectionObserver((entries) => {
    const visible = entries
      .filter((entry) => entry.isIntersecting)
      .sort((a, b) => b.intersectionRatio - a.intersectionRatio)[0];
    if (!visible) return;
    links.forEach((link) => {
      const active = link.getAttribute('href') === `#${visible.target.id}`;
      if (active) link.setAttribute('aria-current', 'true');
      else link.removeAttribute('aria-current');
    });
  }, { rootMargin: '-25% 0px -60%', threshold: [0.05, 0.2, 0.5] });

  sections.forEach((section) => observer.observe(section));
}

if (header) {
  const updateHeader = () => header.classList.toggle('scrolled', window.scrollY > 10);
  updateHeader();
  window.addEventListener('scroll', updateHeader, { passive: true });
}

const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
const updateSvgMotion = (event) => {
  document.querySelectorAll('.gesture-map svg').forEach((svg) => {
    if (event.matches) svg.pauseAnimations();
    else svg.unpauseAnimations();
  });
};
updateSvgMotion(reducedMotion);
reducedMotion.addEventListener('change', updateSvgMotion);

const contactTrigger = document.querySelector('[data-contact-trigger]');
const contactDialog = document.querySelector('[data-contact-dialog]');
const contactAddress = document.querySelector('[data-contact-address]');
const contactCopy = document.querySelector('[data-contact-copy]');
const contactClose = document.querySelector('[data-contact-close]');
const contactFeedback = document.querySelector('[data-contact-feedback]');
const contactEmail = String.fromCharCode(108, 117, 99, 105, 111, 46, 109, 97, 116, 101, 109, 97, 64, 103, 109, 97, 105, 108, 46, 99, 111, 109);

if (contactTrigger && contactDialog && contactAddress) {
  contactTrigger.addEventListener('click', (event) => {
    event.preventDefault();
    contactAddress.textContent = contactEmail;
    contactFeedback.textContent = '';
    contactDialog.showModal();
  });

  contactClose?.addEventListener('click', () => contactDialog.close());

  contactCopy?.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(contactEmail);
      contactFeedback.textContent = 'Email copied.';
    } catch {
      contactFeedback.textContent = 'Select and copy the address above.';
    }
  });
}
