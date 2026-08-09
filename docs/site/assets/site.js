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

const contactLink = document.querySelector('[data-contact-email]');
if (contactLink) {
  contactLink.addEventListener('click', (event) => {
    event.preventDefault();
    const address = String.fromCharCode(108, 117, 99, 105, 111, 46, 109, 97, 116, 101, 109, 97, 64, 103, 109, 97, 105, 108, 46, 99, 111, 109);
    window.open(`https://mail.google.com/mail/?view=cm&fs=1&to=${encodeURIComponent(address)}`, '_blank', 'noopener');
  });
}
