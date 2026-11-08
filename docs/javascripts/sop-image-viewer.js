/* Original SOP screenshots: accessible, local, lossless image viewing. */
(() => {
  const words = {
    zh: { title: '截图原图', fit: '适应窗口', original: '原图 100%', minus: '缩小', plus: '放大', close: '关闭', loading: '正在载入原图…', error: '原图未能载入，请关闭后重新打开。' },
    en: { title: 'Original screenshot', fit: 'Fit to window', original: 'Original 100%', minus: 'Zoom out', plus: 'Zoom in', close: 'Close', loading: 'Loading original image…', error: 'Could not load the image. Close and open it again.' }
  };
  document.addEventListener('click', event => {
    if (event.button !== 0 || event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;
    const anchor = event.target.closest?.('.md-typeset a');
    if (!anchor?.querySelector('img') || !anchor.closest('article')) return;
    const source = new URL(anchor.href, location.href);
    if (source.origin !== location.origin || !/\/assets\/sop\/[^/]+\.(?:png|jpe?g)$/i.test(source.pathname) || !HTMLDialogElement.prototype.showModal) return;
    event.preventDefault();
    const copy = words[/\/en\/sop(?:\/|$)/.test(location.pathname) ? 'en' : 'zh'];
    const dialog = document.createElement('dialog');
    dialog.className = 'sop-image-viewer';
    dialog.setAttribute('aria-label', copy.title);
    const bar = document.createElement('div'); bar.className = 'sop-image-viewer-bar';
    const caption = document.createElement('span'); caption.className = 'sop-image-viewer-caption';
    caption.textContent = anchor.querySelector('img').alt || copy.title;
    bar.append(caption);
    const viewport = document.createElement('div'); viewport.className = 'sop-image-viewer-viewport';
    viewport.tabIndex = 0; viewport.setAttribute('aria-label', copy.title);
    const picture = document.createElement('img'); picture.alt = caption.textContent; picture.draggable = false;
    const status = document.createElement('span'); status.className = 'sop-image-viewer-status';
    status.setAttribute('role', 'status'); status.textContent = copy.loading;
    let scale = 1;
    function draw() {
      if (!picture.naturalWidth) return;
      picture.style.width = Math.round(picture.naturalWidth * scale) + 'px';
      picture.style.height = 'auto';
      status.textContent = picture.naturalWidth + ' × ' + picture.naturalHeight + ' · ' + Math.round(scale * 100) + '%';
    }
    function fit() {
      if (!picture.naturalWidth) return;
      scale = Math.min(1, (viewport.clientWidth - 24) / picture.naturalWidth, (viewport.clientHeight - 24) / picture.naturalHeight);
      draw(); viewport.scrollTo(0, 0);
    }
    function button(label, action) {
      const control = document.createElement('button'); control.type = 'button'; control.textContent = label;
      control.addEventListener('click', action); bar.append(control); return control;
    }
    button(copy.fit, fit);
    button(copy.original, () => { scale = 1; draw(); viewport.scrollTo(0, 0); });
    button('−', () => { scale = Math.max(.1, scale / 1.25); draw(); }).setAttribute('aria-label', copy.minus);
    button('+', () => { scale = Math.min(4, scale * 1.25); draw(); }).setAttribute('aria-label', copy.plus);
    const close = button(copy.close, () => dialog.close());
    viewport.append(picture); dialog.append(bar, viewport, status); document.body.append(dialog);
    dialog.addEventListener('close', () => { window.removeEventListener('resize', fit); dialog.remove(); if (anchor.isConnected) anchor.focus({ preventScroll: true }); }, { once: true });
    dialog.addEventListener('click', e => { if (e.target === dialog) dialog.close(); });
    picture.addEventListener('load', fit, { once: true });
    picture.addEventListener('error', () => { status.textContent = copy.error; }, { once: true });
    dialog.showModal(); close.focus(); picture.src = source.href; window.addEventListener('resize', fit);
  });
})();
