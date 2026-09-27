const form = document.getElementById("login-form");
const status = document.getElementById("login-status");
form.addEventListener("submit", async (event) => {
  event.preventDefault();
  const bootstrapCode = document.getElementById("bootstrap-code").value;
  const response = await fetch("/api/v1/session", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ bootstrap_code: bootstrapCode }),
  });
  if (response.ok) location.replace("/");
  else status.textContent = "Code refusé ou session indisponible.";
});
