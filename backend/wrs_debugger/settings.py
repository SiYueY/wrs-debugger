import json
import os
from pathlib import Path


class SettingsRepository:
    """Persists only the Receiver DDS domain setting."""

    def __init__(self, path: Path | None = None) -> None:
        config_home = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
        self.path = path or config_home / "wrs-debugger" / "settings.json"

    def load_domain_id(self) -> int:
        try:
            value = json.loads(self.path.read_text(encoding="utf-8"))
            domain_id = value.get("domain_id")
            return domain_id if isinstance(domain_id, int) and domain_id >= 0 else 0
        except (OSError, ValueError, json.JSONDecodeError):
            return 0

    def save_domain_id(self, domain_id: int) -> None:
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            self.path.write_text(json.dumps({"domain_id": domain_id}), encoding="utf-8")
        except OSError:
            # Optional local settings must not prevent device functionality.
            return
