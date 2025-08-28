document.getElementById('submit-button').addEventListener('click', async () => {
  const eq = document.getElementById('eq').value;
  let res = await fetch('/equation/balance', {
    method: 'POST',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({equation: eq})
  });
  let data = await res.json();
  if (!res.ok) {
    let err = document.getElementById('err-msg');
    err.style.display = 'block';
    err.textContent = `Error: ${data.error}`;
    return;
  }
  document.getElementById('balanced').textContent = data.balanced;
});