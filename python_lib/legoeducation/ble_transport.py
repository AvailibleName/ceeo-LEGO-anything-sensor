"""Pluggable BLE transport interface and registry.

End users can supply their own BLE implementation (e.g. non-bleak backend
for unsupported platforms) by either:

1. Setting the environment variable LEGOEDUCATION_BLE_IMPL to
   "package.module:ClassName" before importing the API, OR
2. Calling register_transport(CustomClass) early in application startup.

Custom transport classes MUST subclass BLETransport and implement all
abstract methods. The Worker/background thread will instantiate the
registered transport with a shutdown_callback.

Contract summary (all coroutines run inside the worker thread's asyncio loop):
    scan_devices(timeout: float, filters: dict|None) -> list|None
        Return a list of device objects (backend-defined) or None.
    connect(device, notification_callback, disconnect_callback) -> bool
        Start notifications; call notification_callback(characteristic, data_bytes)
        for each incoming packet. Return True on success.
    send(device, message: bytes) -> None
        Write raw bytes to the connected device.
    device_disconnect(device) -> None
        Disconnect a single device gracefully.
    shutdown_all() -> None
        Disconnect all devices and invoke provided shutdown callback.

Edge cases & expectations:
    - Methods must be resilient to being called with devices that are
      partially connected or already disconnected (idempotent disconnect).
    - shutdown_all must invoke the shutdown_callback even if no clients remain.
    - Exceptions should be caught internally and logged; raising is treated
      by Worker as fatal.
"""

import abc
import logging
import os
import importlib
from typing import Any, Callable, List, Optional, Type


class BLETransport(abc.ABC):
    def __init__(self, shutdown_callback: Optional[Callable[[], None]] = None):
        self.shutdown_callback = shutdown_callback
        self.logger = logging.getLogger(__name__)

    @abc.abstractmethod
    async def scan_devices(self, timeout: float, filters: Any = None) -> Optional[List[Any]]:  # pragma: no cover - interface
        pass

    @abc.abstractmethod
    async def connect(self, device: Any, notification_callback: Callable, disconnect_callback: Callable) -> bool:  # pragma: no cover - interface
        pass

    @abc.abstractmethod
    async def send(self, device: Any, message: bytes) -> None:  # pragma: no cover - interface
        pass

    @abc.abstractmethod
    async def device_disconnect(self, device: Any) -> None:  # pragma: no cover - interface
        pass

    @abc.abstractmethod
    def shutdown_all(self) -> None:  # pragma: no cover - interface
        pass

    def request_cancel(self):
        """Request cancellation of in-progress operations.

        Concrete no-op so custom transports that don't need cancellation
        still work when the Worker calls this during close_thread().
        Override in subclasses to support graceful Ctrl+C shutdown."""
        pass

    def reset_cancel(self):
        """Clear prior cancellation state before a new operation.

        Called by the worker loop before every request dispatch.
        Override in subclasses that implement request_cancel()."""
        pass


_transport_class: Optional[Type[BLETransport]] = None


def register_transport(cls: Type[BLETransport]) -> None:
    """Register a BLE transport implementation. Must be called before Worker creation.
    Subsequent registrations replace the previous implementation."""
    global _transport_class
    if not issubclass(cls, BLETransport):  # defensive check
        raise TypeError("Transport must inherit BLETransport")
    _transport_class = cls


def get_transport() -> Type[BLETransport]:
    """Return the currently registered transport class.

    Lazy fallback: if nothing is registered yet, attempt importing the
    default bleak-based implementation (basic_ble.BasicBLE). This supports
    use-cases where users import a high-level hub module that eventually
    triggers Worker creation before basic_ble was imported indirectly.

    If import fails (e.g. bleak missing), we keep the original error for
    clarity. Users can still register a custom transport manually before
    creating any hub instances.
    """
    global _transport_class

    if _transport_class is None:
        _check_env_override()
        if _transport_class is None:
            try:
                from . import basic_ble
                register_transport(basic_ble.BasicBLE)
            except Exception:
                raise RuntimeError("No BLE transport registered and default import failed. Install bleak or register a custom transport.")
    return _transport_class


def _check_env_override() -> None:
    """Environment-based override mechanism. Allows users to set
    LEGOEDUCATION_BLE_IMPL="pkg.module:ClassName" prior to import.
    Errors are logged and ignored (fallback to previously registered class)."""
    spec = os.getenv("LEGOEDUCATION_BLE_IMPL")
    if not spec:
        return
    logger = logging.getLogger(__name__)
    try:
        module_name, class_name = spec.split(":", 1)
        module = importlib.import_module(module_name)
        cls = getattr(module, class_name)
        register_transport(cls)
        logger.info("Custom BLE transport loaded: %s", spec)
    except Exception as exc:  # pragma: no cover - robustness
        logger.error("Failed to load custom BLE transport '%s': %s", spec, exc)
