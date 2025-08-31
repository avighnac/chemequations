function set_placeholder() {
  let input = document.getElementById('balance-reaction-input');
  if (window.innerWidth <= 750) {
    input.placeholder = 'Enter a reaction: H2 + O2 = H2O...';
  } else {
    input.placeholder = 'Enter a reaction, for example, H2 + O2 = H2O...';
  }
};

document.addEventListener('DOMContentLoaded', set_placeholder);
window.addEventListener('resize', set_placeholder);

let atom_map = {};
(async () => {
  let res = await fetch('/data/atoms');
  let data = await res.json();
  for (const { symbol, number } of data.atoms) {
    atom_map[symbol] = number;
  }
})();

let error_element = document.getElementById('error-message');
let reaction_container = document.getElementById('balanced-output-no-error');
let reaction_element = document.getElementById('balanced-reaction');

function render_error(error) {
  reaction_container.classList.remove('visible');
  error_element.style.display = 'block';
  error_element.textContent = `Error: ${error}`;
}

function parse_compound(compound) {
  // let ans = {}, cur = {};
  // ans[bal] = answer at depth `bal`
  let ans = [], cur = [], elem_mode = [];
  function clean_up_cur(bal, mult) {
    for (const i in cur[bal]) {
      ans[bal][i] = (ans[bal][i] || 0) + mult * cur[bal][i];
    }
  };
  let bal = 0;
  for (let i = 0; i < compound.length; ++i) {
    if (elem_mode.length == bal) {
      ans.push({});
      cur.push({});
      elem_mode.push(true);
    }
    if (compound[i] == '(') {
      clean_up_cur(bal, 1);
      elem_mode[bal++] = false;
      continue;
    }
    if (compound[i] == ')') {
      clean_up_cur(bal, 1);
      bal--;
      if (bal < 0) {
        throw `Compound ${compound} has too many closing parentheses`;
      }
      cur[bal] = ans[bal + 1];
      continue;
    }
    if (elem_mode[bal]) {
      // skip balacing
      // warn the user
      let warned = false;
      while (i < compound.length && !isNaN(parseInt(compound[i]))) {
        if (!warned) {
          warned = true;
          console.warn(`Warning: ignoring extra numbers in ${compound}`);
        }
        i++;
      }
      let valid_len = 0;
      for (let j = 1; j <= 3 && i + j <= compound.length; ++j) {
        if (compound.substring(i, i + j) in atom_map) {
          valid_len = j;
        }
      }
      if (valid_len == 0) {
        throw `Invalid compound: ${compound}`;
      }
      cur[bal][compound.substring(i, i + valid_len)] = 1;
      elem_mode[bal] = false;
      i += valid_len - 1;
    } else {
      let mult = 1;
      if (!isNaN(parseInt(compound[i]))) {
        mult = 0;
        for (; i < compound.length && !isNaN(parseInt(compound[i])); ++i) {
          mult = 10 * mult + Number(compound[i]);
        }
      }
      --i;
      clean_up_cur(bal, mult);
      cur[bal] = {};
      elem_mode[bal] = true;
    }
  }
  if (bal != 0) {
    throw `Compound ${compound} does not have enough closing parentheses`;
  }
  clean_up_cur(bal, 1);
  return ans[bal];
}

function parse_reaction(reaction) {
  reaction = reaction.replaceAll(' ', '').replaceAll('−', '-').replace(/-+/g, '-').replace('->', '=');
  let _compounds = reaction.split('=');
  if (_compounds.length != 2) {
    return render_error(`Expected exactly 1 =, found ${_compounds.length - 1}`);
  }
  let compounds = [];
  let lhs = _compounds[0].split('+').map(x => x.replace(/^\d+/, ''));
  let rhs = _compounds[1].split('+').map(x => x.replace(/^\d+/, ''));
  try {
    for (const i of lhs) {
      compounds.push(parse_compound(i));
    }
    for (const i of rhs) {
      let compound = parse_compound(i);
      for (const j in compound) {
        compound[j] = -compound[j];
      }
      compounds.push(compound);
    }
  } catch (err) {
    return render_error(err);
  }
  let seen = {};
  for (i of compounds) {
    for (j in i) {
      seen[j] = true;
    }
  }
  let matrix = [];
  Object.keys(seen).forEach((e, i) => {
    matrix.push([]);
    for (let j = 0; j < compounds.length; ++j) {
      matrix[i][j] = new Fraction(compounds[j][e] || 0);
    }
  });
  return { matrix, lhs, rhs };
}

function render_reaction(reaction) {
  reaction_container.classList.remove('visible');
  requestAnimationFrame(() => { });
  reaction_element.textContent = `\\(\\ce{${reaction}}\\)`;
  MathJax.typesetPromise([reaction_element]).then(() => {
    error_element.style.display = 'none';
    setTimeout(() => {
      reaction_container.classList.add('visible');
    }, 100);
  });
}

let input = document.getElementById('balance-reaction-input');

document.getElementById('button-balance').addEventListener('click', () => {
  let { matrix, lhs, rhs } = parse_reaction(input.value) || {};
  if (!matrix) {
    return;
  }
  if (matrix[0].length - 1 > matrix.length) {
    return render_error('There are less equations than compounds: unable to balance');
  }
  // do gaussian elimination
  for (let i = 0; i < matrix[0].length - 1; ++i) {
    let j = matrix.findIndex((arr, _i) => {
      return _i >= i && arr[i] != 0;
    });
    if (j == -1) {
      return render_error('System of equations is unsolvable');
    }
    [matrix[i], matrix[j]] = [matrix[j], matrix[i]];
    let fac = matrix[i][i];
    matrix[i] = matrix[i].map(e => e.div(fac));
    for (let j = i + 1; j < matrix.length; ++j) {
      // matrix[j] -= matrix[j][i] * matrix[i];
      matrix[j] = matrix[j].map((e, k) => e.sub(matrix[j][i].mul(matrix[i][k])));
    }
  }
  for (let i = matrix[0].length - 2; i >= 0; --i) {
    // matrix[j] -= matrix[j][i] * matrix[i];
    for (let j = i - 1; j >= 0; --j) {
      matrix[j] = matrix[j].map((e, k) => e.sub(matrix[j][i].mul(matrix[i][k])));
    }
  }
  let coeffs = [];
  let gcd = new Fraction(0);
  for (let i = 0; i < matrix[0].length - 1; ++i) {
    coeffs.push(matrix[i].at(-1).mul(-1));
    gcd = gcd.gcd(coeffs[i]);
  }
  coeffs.push(new Fraction(1));
  coeffs.push(new Fraction(1));
  // check for contradictions
  for (let i = matrix[0].length - 1; i < matrix.length; ++i) {
    let val = Fraction(0);
    matrix[i].forEach((e, i) => val = val.add(e.mul(coeffs[i])));
    if (val.toString() != '0') {
      return render_error('System of equations contains contradictions');
    }
  }
  for (let i = 0; i < coeffs.length; ++i) {
    coeffs[i] = coeffs[i].div(gcd);
  }
  if (coeffs.findIndex(e => e.toString() == '0') != -1) {
    return render_error('Reaction cannot be balanced');
  }
  let ans = '';
  for (let i = 0; i < lhs.length; ++i) {
    if (coeffs[i].toString() != '1') {
      ans += coeffs[i].toString() + ' ';
    }
    ans += lhs[i];
    if (i != lhs.length - 1) {
      ans += ' + ';
    }
  }
  ans += ' -> ';
  for (let i = 0; i < rhs.length; ++i) {
    let coeff = coeffs[lhs.length + i];
    if (coeff.toString() != '1') {
      ans += coeff.toString() + ' ';
    }
    ans += rhs[i];
    if (i != rhs.length - 1) {
      ans += ' + ';
    }
  }
  console.log(ans);
  render_reaction(ans);
});