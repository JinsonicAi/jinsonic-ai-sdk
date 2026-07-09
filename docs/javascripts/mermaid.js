const getMermaidTheme = () =>
  document.body.getAttribute("data-md-color-scheme") === "slate"
    ? "dark"
    : "default";

document$.subscribe(async () => {
  mermaid.initialize({
    startOnLoad: false,
    theme: getMermaidTheme(),
    securityLevel: "strict",
  });

  await mermaid.run({
    nodes: document.querySelectorAll(".mermaid"),
  });
});