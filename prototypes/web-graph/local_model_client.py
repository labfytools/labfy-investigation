"""Client HTTP minimal pour un modèle OpenAI-compatible strictement local."""

from __future__ import annotations

import http.client
import json
import socket
from urllib.parse import urlsplit


class LocalModelError(RuntimeError):
    """Erreur de base du fournisseur local."""


class LocalModelUnavailable(LocalModelError):
    """Le fournisseur local ne peut pas répondre correctement."""


class LocalModelResponseError(LocalModelError):
    """Le fournisseur a répondu avec une enveloppe invalide."""


class LocalModelClient:
    MAX_REQUEST_BYTES = 128 * 1024
    MAX_RESPONSE_BYTES = 256 * 1024

    def __init__(self, endpoint, model, *, timeout=10.0,
                 max_response_bytes=MAX_RESPONSE_BYTES):
        parts = self._validate_endpoint(endpoint)
        if not isinstance(model, str) or not model.strip() or len(model) > 256:
            raise ValueError("Identifiant de modèle invalide")
        if not isinstance(timeout, (int, float)) or isinstance(timeout, bool):
            raise ValueError("Délai modèle invalide")
        if timeout < 0.1 or timeout > 30.0:
            raise ValueError("Délai modèle hors limites")
        if (not isinstance(max_response_bytes, int) or isinstance(max_response_bytes, bool)
                or max_response_bytes < 1024
                or max_response_bytes > self.MAX_RESPONSE_BYTES):
            raise ValueError("Limite de réponse modèle invalide")
        self.endpoint = endpoint
        self.model = model
        self.timeout = float(timeout)
        self.max_response_bytes = max_response_bytes
        self._host = parts.hostname
        self._port = parts.port

    @staticmethod
    def _validate_endpoint(endpoint):
        if not isinstance(endpoint, str):
            raise ValueError("Endpoint modèle invalide")
        try:
            parts = urlsplit(endpoint)
            port = parts.port
        except ValueError as error:
            raise ValueError("Endpoint modèle invalide") from error
        if (
            parts.scheme != "http"
            or parts.hostname not in {"127.0.0.1", "::1"}
            or port is None
            or not 1 <= port <= 65535
            or parts.username is not None
            or parts.password is not None
            or parts.query
            or parts.fragment
            or parts.path not in {"", "/"}
        ):
            raise ValueError("Endpoint modèle limité au loopback HTTP")
        # INVARIANT: netloc doit être canonique pour exclure les suffixes ambigus.
        canonical = f"127.0.0.1:{port}" if parts.hostname == "127.0.0.1" else f"[::1]:{port}"
        if parts.netloc != canonical or endpoint not in {f"http://{canonical}", f"http://{canonical}/"}:
            raise ValueError("Endpoint modèle non canonique")
        return parts

    def complete(self, messages):
        if not isinstance(messages, list) or not messages:
            raise ValueError("Messages modèle invalides")
        request_value = {
            "model": self.model,
            "messages": messages,
            "temperature": 0,
            "stream": False,
        }
        try:
            request_body = json.dumps(
                request_value, ensure_ascii=False, separators=(",", ":")
            ).encode("utf-8")
        except (TypeError, ValueError) as error:
            raise ValueError("Messages modèle non sérialisables") from error
        if len(request_body) > self.MAX_REQUEST_BYTES:
            raise ValueError("Requête modèle trop volumineuse")

        connection = http.client.HTTPConnection(self._host, self._port, timeout=self.timeout)
        try:
            # WHY: HTTPConnection contacte directement le loopback et n'utilise
            # ni variables de proxy, ni redirections, ni magasin de cookies/netrc.
            connection.request(
                "POST",
                "/v1/chat/completions",
                body=request_body,
                headers={"Content-Type": "application/json", "Accept": "application/json"},
            )
            response = connection.getresponse()
            if response.status != 200:
                raise LocalModelUnavailable(f"Réponse HTTP modèle inattendue: {response.status}")
            content_type = response.getheader("Content-Type", "").split(";", 1)[0].strip().lower()
            if content_type != "application/json":
                raise LocalModelResponseError("Content-Type modèle invalide")
            content_length = response.getheader("Content-Length")
            if content_length is not None:
                try:
                    declared_length = int(content_length)
                except ValueError as error:
                    raise LocalModelResponseError("Content-Length modèle invalide") from error
                if declared_length < 0 or declared_length > self.max_response_bytes:
                    raise LocalModelResponseError("Réponse modèle trop volumineuse")
            body = response.read(self.max_response_bytes + 1)
            if len(body) > self.max_response_bytes:
                raise LocalModelResponseError("Réponse modèle trop volumineuse")
        except LocalModelError:
            raise
        except (OSError, socket.timeout, http.client.HTTPException) as error:
            raise LocalModelUnavailable("Fournisseur modèle local indisponible") from error
        finally:
            connection.close()

        try:
            value = json.loads(body.decode("utf-8"))
            choices = value["choices"]
            content = choices[0]["message"]["content"]
        except (UnicodeError, json.JSONDecodeError, KeyError, IndexError, TypeError) as error:
            raise LocalModelResponseError("Réponse OpenAI-compatible invalide") from error
        if not isinstance(choices, list) or len(choices) != 1 or not isinstance(content, str):
            raise LocalModelResponseError("Choix modèle invalide")
        return content
