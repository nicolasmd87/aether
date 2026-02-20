// Index page search - shows results, hides module grid
function search() {
  const query = document.getElementById('search').value.toLowerCase().trim();
  const results = document.getElementById('search-results');
  const grid = document.querySelector('.module-grid');
  const items = document.querySelectorAll('.function-item');
  
  if (query.length === 0) {
    results.classList.add('hidden');
    grid.classList.remove('hidden');
    return;
  }
  
  results.classList.remove('hidden');
  grid.classList.add('hidden');
  
  items.forEach(item => {
    const name = item.dataset.name.toLowerCase();
    item.classList.toggle('hidden', !name.includes(query));
  });
}

// Module page search - filters functions
function searchModule() {
  const query = document.getElementById('search').value.toLowerCase().trim();
  const functions = document.querySelectorAll('.function');
  const navItems = document.querySelectorAll('.function-nav li');
  
  functions.forEach(func => {
    const name = func.dataset.name.toLowerCase();
    func.classList.toggle('hidden', query.length > 0 && !name.includes(query));
  });
  
  navItems.forEach(item => {
    const link = item.querySelector('a');
    if (link) {
      const name = link.textContent.toLowerCase();
      item.classList.toggle('hidden', query.length > 0 && !name.includes(query));
    }
  });
}

// Highlight active function on scroll
let ticking = false;
document.addEventListener('scroll', () => {
  if (!ticking) {
    requestAnimationFrame(() => {
      const functions = document.querySelectorAll('.function');
      const navLinks = document.querySelectorAll('.function-nav a');
      let current = '';
      functions.forEach(func => {
        const rect = func.getBoundingClientRect();
        if (rect.top <= 80) current = func.id;
      });
      navLinks.forEach(link => {
        link.parentElement.classList.remove('active');
        if (link.getAttribute('href') === '#' + current) {
          link.parentElement.classList.add('active');
        }
      });
      ticking = false;
    });
    ticking = true;
  }
});
