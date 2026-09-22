import json
import os
import tempfile
from pathlib import Path

from pydantic_settings import BaseSettings, SettingsConfigDict


class BackendSettings(BaseSettings):
    model_config = SettingsConfigDict(env_prefix="WRS_DEBUGGER_", extra="ignore")

    build_id: str = "dev"


class SettingsRepository:
    """Persist only user-owned Receiver connection settings."""

    def __init__(self, path: Path | None = None) -> None:
        config_home = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
        self.path = path or config_home / "wrs-debugger" / "settings.json"

    def load_domain_id(self) -> int:
        try:
            value = json.loads(self.path.read_text(encoding="utf-8"))
        except (OSError, ValueError, json.JSONDecodeError):
            return 0
        if not isinstance(value, dict):
            return 0
        receiver = value.get("receiver")
        if isinstance(receiver, dict):
            domain_id = receiver.get("domain_id")
        else:
            domain_id = value.get("domain_id")
        return domain_id if isinstance(domain_id, int) and domain_id >= 0 else 0

    def save_domain_id(self, domain_id: int) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        payload = json.dumps({"receiver": {"domain_id": domain_id}}, ensure_ascii=False)
        descriptor, temporary_name = tempfile.mkstemp(
            dir=self.path.parent,
            prefix=f".{self.path.name}.",
            suffix=".tmp",
        )
        temporary_path = Path(temporary_name)
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8") as stream:
                stream.write(payload)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary_path, self.path)
        except Exception:
            temporary_path.unlink(missing_ok=True)
            raise
