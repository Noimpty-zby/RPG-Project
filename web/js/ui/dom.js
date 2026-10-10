// Small DOM toolkit: element builder, modal stack with keyboard shortcuts,
// toasts and screen fades.
import { sfx } from '../engine/audio.js';

export function h(tag, attrs = {}, ...children) {
  const el = document.createElement(tag);
  for (const [key, value] of Object.entries(attrs || {})) {
    if (value === undefined || value === null || value === false) continue;
    if (key === 'class') el.className = value;
    else if (key === 'style' && typeof value === 'object') Object.assign(el.style, value);
    else if (key.startsWith('on') && typeof value === 'function') el.addEventListener(key.slice(2), value);
    else if (key === 'html') el.innerHTML = value;
    else if (value === true) el.setAttribute(key, '');
    else el.setAttribute(key, value);
  }
  append(el, children);
  return el;
}
function append(el, children) {
  for (const child of children.flat(Infinity)) {
    if (child === null || child === undefined || child === false) continue;
    el.append(child instanceof Node ? child : document.createTextNode(String(child)));
  }
}

// A button with an optional hotkey badge and sub-line.
export function button(label, onClick, o = {}) {
  const el = h('button', {
    class: `btn ${o.cls || ''}`.trim(), disabled: o.disabled, title: o.title, 'data-hotkey': o.key,
    onclick: (e) => {
      e.stopPropagation();
      if (el.disabled) return;
      sfx(o.sound || 'click');
      onClick?.(e);
    },
  }, o.key ? h('span', { class: 'key' }, o.keyLabel || o.key) : null, label, o.sub ? h('span', { class: 'sub' }, o.sub) : null);
  return el;
}

export function bar(value, max, cls = '', text = null) {
  const pct = max > 0 ? Math.max(0, Math.min(100, (value / max) * 100)) : 0;
  return h('div', { class: `bar ${cls}` }, h('b', { style: { width: `${pct}%` } }), h('i', { style: { width: `${pct}%` } }),
    h('span', {}, text ?? `${value}/${max}`));
}
// Update an existing bar in place so the damage-lag animation plays.
export function updateBar(el, value, max, text = null) {
  const pct = max > 0 ? Math.max(0, Math.min(100, (value / max) * 100)) : 0;
  el.querySelector('i').style.width = `${pct}%`;
  el.querySelector('b').style.width = `${pct}%`;
  el.querySelector('span').textContent = text ?? `${value}/${max}`;
}

// ---- Modals ------------------------------------------------------------------------
const stack = [];
let inputRef = null;
export function bindInput(input) { inputRef = input; }
export const modalOpen = () => stack.length > 0;

export function openModal(o) {
  const root = document.getElementById('modals');
  const backdrop = h('div', { class: 'modal-backdrop' });
  const modal = h('div', { class: `modal panel ${o.size || ''}`, role: 'dialog', 'aria-label': o.title });
  const header = h('header', {}, h('h2', {}, o.title));
  if (o.closable !== false) header.append(button('关闭', () => api.close(), { cls: 'small close', key: 'Esc', keyLabel: 'Esc' }));
  const tabs = h('div', { class: 'tabs' });
  const body = h('div', { class: 'body' });
  const footer = h('footer');
  modal.append(header);
  if (o.tabs) modal.append(tabs);
  modal.append(body, footer);
  backdrop.append(modal);
  if (o.closable !== false) backdrop.addEventListener('pointerdown', (e) => { if (e.target === backdrop) api.close(); });
  root.append(backdrop);
  let activeTab = o.initialTab ?? 0;
  const api = {
    el: modal,
    body,
    closed: false,
    tab: () => activeTab,
    setTab(index) { activeTab = index; api.refresh(); },
    refresh() {
      if (api.closed) return;
      const scroll = body.scrollTop;
      body.replaceChildren();
      footer.replaceChildren();
      if (o.tabs) {
        tabs.replaceChildren(...o.tabs.map((label, i) =>
          button(label, () => api.setTab(i), { cls: `small ${i === activeTab ? 'active' : ''}` })));
      }
      const content = o.render ? o.render(api) : o.body;
      if (content) append(body, [content]);
      const buttons = typeof o.buttons === 'function' ? o.buttons(api) : o.buttons || [];
      for (const b of buttons) footer.append(button(b.label, () => b.onClick(api), b));
      footer.classList.toggle('hidden', buttons.length === 0);
      body.scrollTop = scroll;
    },
    close(result) {
      if (api.closed) return;
      api.closed = true;
      backdrop.remove();
      stack.splice(stack.indexOf(api), 1);
      if (!stack.length && inputRef) { inputRef.enabled = true; inputRef.clear(); }
      sfx('close');
      o.onClose?.(result);
    },
  };
  stack.push(api);
  if (inputRef) { inputRef.enabled = false; inputRef.clear(); }
  sfx('open');
  api.refresh();
  setTimeout(() => {
    const first = modal.querySelector('footer .btn.primary, .body .btn:not(:disabled), footer .btn');
    if (first && matchMedia('(pointer: fine)').matches) first.focus({ preventScroll: true });
  }, 30);
  return api;
}

// Promise-based choice dialog: resolves with the chosen value (or null when closed).
export function choose(title, content, options, o = {}) {
  return new Promise((resolve) => {
    let chosen = null;
    const modal = openModal({
      title, size: o.size || 'narrow', closable: o.closable ?? true,
      render: () => (typeof content === 'function' ? content() : content),
      buttons: options.map((opt, i) => ({ ...opt, key: opt.key ?? String(i + 1),
        onClick: () => { chosen = opt.value; modal.close(); } })),
      onClose: () => resolve(chosen),
    });
  });
}
export const alertBox = (title, content, label = '确定') =>
  choose(title, content, [{ label, value: true, cls: 'primary', key: 'Enter', keyLabel: '⏎' }]);

addEventListener('keydown', (e) => {
  const top = stack[stack.length - 1];
  if (!top) return;
  if (e.target instanceof HTMLInputElement) {
    if (e.key === 'Escape') e.target.blur();
    return;
  }
  if (e.key === 'Escape') {
    const closeBtn = top.el.querySelector('header .close');
    if (closeBtn) { e.preventDefault(); top.close(); }
    return;
  }
  const key = e.key === 'Enter' ? 'Enter' : e.key;
  const target = [...top.el.querySelectorAll('[data-hotkey]')].find((b) => b.dataset.hotkey === key && !b.disabled);
  if (target && !(key === 'Enter' && document.activeElement?.classList.contains('btn'))) {
    e.preventDefault();
    target.click();
  }
});

// ---- Toasts and fades -----------------------------------------------------------------
export function toast(text, cls = '') {
  const el = h('div', { class: `toast panel ${cls}` }, text);
  document.getElementById('toasts').append(el);
  setTimeout(() => el.remove(), 3500);
}
export async function fade(during) {
  const el = document.getElementById('fade');
  el.classList.add('on');
  await new Promise((r) => setTimeout(r, 360));
  await during?.();
  el.classList.remove('on');
  await new Promise((r) => setTimeout(r, 200));
}
