(function () {
  "use strict";

  if (window.location.hostname !== "aibox-sdk-docs.readthedocs.io") {
    return;
  }

  const beacon = document.createElement("script");
  beacon.type = "module";
  beacon.async = true;
  beacon.src = "https://static.cloudflareinsights.com/beacon.min.js";
  beacon.dataset.cfBeacon = JSON.stringify({
    token: "cf67c8616fc240e4bbd7eaf039e50d65"
  });
  document.head.appendChild(beacon);
})();