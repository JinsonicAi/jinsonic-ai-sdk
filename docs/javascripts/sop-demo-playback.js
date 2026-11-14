// The three examples are embedded beside their instructions. Native controls
// stay available when the browser or reader chooses manual playback.
(() => {
  function init() {
    const demos = document.querySelectorAll('video[data-sop-demo]');
    if (!demos.length || !('IntersectionObserver' in window)) return;

    const reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
    for (const video of demos) {
      let inView = false;
      let readerPaused = false;
      let automaticPauses = 0;
      function pauseForPage() {
        if (!video.paused) {
          automaticPauses++;
          video.pause();
        }
      }
      function update() {
        if (!inView || document.hidden || reducedMotion.matches) {
          pauseForPage();
        } else if (!readerPaused && video.paused) {
          video.play().catch(() => {
            // Native play remains available if the browser declines autoplay.
          });
        }
      }
      video.addEventListener('pause', () => {
        if (automaticPauses) automaticPauses--;
        else if (!video.ended) readerPaused = true;
      });
      video.addEventListener('play', () => { readerPaused = false; });
      const observer = new IntersectionObserver(([entry]) => {
        const next = inView ? entry.intersectionRatio > 0.1 : entry.intersectionRatio >= 0.5;
        if (next !== inView) {
          inView = next;
          update();
        }
      }, { threshold: [0, 0.1, 0.5, 1] });
      observer.observe(video);
      document.addEventListener('visibilitychange', update);
      reducedMotion.addEventListener('change', update);
      window.addEventListener('pagehide', pauseForPage);
      window.addEventListener('pageshow', update);
    }
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init, { once: true });
  else init();
})();
