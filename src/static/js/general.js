let button_dropdown = document.getElementById('button-dropdown');
let dropdown = document.getElementById('dropdown-header-container');
button_dropdown.addEventListener('click', () => {
  if (dropdown.classList.contains('collapsed')) {
    dropdown.classList.remove('collapsed');
  } else {
    dropdown.classList.add('collapsed');
  }
});

window.addEventListener('resize', () => {
  if (window.innerWidth > 650 && dropdown.classList.contains('collapsed')) {
    dropdown.classList.remove('collapsed');
  }
});