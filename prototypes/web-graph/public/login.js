const form = document.getElementById("login-form");
const status = document.getElementById("login-status");
form.addEventListener("submit", async (event) => {
  event.preventDefault();
  const input = document.getElementById("bootstrap-code");
  const button = form.querySelector("button");
  button.disabled = true;
  status.textContent = "Ouverture de la session locale…";
  try {
    const response = await fetch("/api/v1/session", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ bootstrap_code: input.value }),
    });
    // INVARIANT: le secret saisi ne rejoint ni l'URL, ni le stockage Web,
    // ni un message d'erreur rendu à l'écran.
    input.value = "";
    if (!response.ok) throw new Error("Code refusé ou session indisponible.");
    location.replace("/");
  } catch (error) {
    input.value = "";
    status.textContent = error.message === "Failed to fetch"
      ? "Le service local ne répond pas. Vérifiez qu’il est toujours lancé."
      : error.message;
    button.disabled = false;
    input.focus();
  }
});
