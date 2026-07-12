import queue
import threading
import asyncio
import logging
import time

from .ble_transport import get_transport

class Worker:
    def __init__(self):
        self.request_queue = asyncio.Queue()
        self.result_queue = queue.Queue()
        self.logger = logging.getLogger(__name__)
        self.loop = None
        self._myble = None
        # Event used to signal that the asyncio loop in the BLE worker thread is ready
        self.loop_ready = threading.Event()
        self.ble_thread = None
        self._thread_lock = threading.Lock()
        # Re-entrancy guard: prevents concurrent close_thread() calls from
        # racing (e.g. KeyboardInterrupt handler + atexit both triggering shutdown).
        self._closing = False

    def cancel_current_operation(self):
        """Cancel any in-progress BLE operation (scan/connect)."""
        ble = self._myble
        if ble is not None:
            try:
                ble.request_cancel()
            except Exception:
                self.logger.debug("Failed to request cancel on BLE transport", exc_info=True)
    
    def start_thread(self):
        with self._thread_lock:
            if self.ble_thread and self.ble_thread.is_alive():
                return

            if self.ble_thread and not self.ble_thread.is_alive():
                self.ble_thread = None

            if self.ble_thread is None:
                self.ble_thread = threading.Thread(target=self.ble_worker, args=(self.request_queue, self.result_queue))
                self.ble_thread.daemon = True  # Make thread daemon so it doesn't prevent exit
                self.ble_thread.start()

    def close_thread(self):
        with self._thread_lock:
            if self._closing:
                return
            self._closing = True
            thread = self.ble_thread
            if thread is None:
                self._closing = False
                return
            # If thread already finished, just clear reference and return
            if not thread.is_alive():
                self.ble_thread = None
                self._closing = False
                return

            # Signal the transport to abort any in-flight scan/connect so
            # the worker loop can drain quickly and reach the None sentinel.
            self.cancel_current_operation()

            # Only attempt to submit shutdown sentinel if the loop was initialized.
            if self.loop_ready.is_set():
                self.put_request(None)
            else:
                # Loop wasn't ever marked ready (startup race or already terminated). We'll just join.
                self.logger.debug("BLE worker loop not ready/cleared; skipping shutdown sentinel")

        # join() outside the lock so the worker thread can still acquire it
        # if needed. The finally block guarantees _closing is reset even if
        # join() raises (e.g. timeout on a stuck thread).
        try:
            if thread.is_alive():
                self.logger.info("Waiting for BLE thread to finish")
            thread.join(timeout=3)
            if thread.is_alive():
                self.logger.warning("BLE thread did not finish in time")
            else:
                self.logger.info("BLE thread completed")
        finally:
            with self._thread_lock:
                self.ble_thread = None
                self._closing = False

    async def async_put_request(self, request):
        await self.request_queue.put(request)

    def put_request(self, request):
        # Wait for the worker thread to set up its asyncio loop.
        if not self.loop_ready.wait(timeout=3):
            raise RuntimeError("BLE worker loop not ready")
        asyncio.run_coroutine_threadsafe(self.async_put_request(request), self.loop)

    def ble_worker(self, request_queue, result_queue):
    
        logger = self.logger

        def parse(req, d1 = None, d2 = None, d3 = None, d4 = None):
            return req.get('msg', d1), req.get('msg2', d2), req.get('msg3', d3), req.get('msg4', d4)

        async def worker_loop():
            def ble_shutdown_callback():
                nonlocal running
                result_queue.put('done')    
                running = False
                logger.info("BLE worker shutdown requested")
                request_queue.put_nowait(None)

            transport_cls = get_transport()
            myble = transport_cls(shutdown_callback=ble_shutdown_callback)
            self._myble = myble
            self.loop = asyncio.get_running_loop()
            # Signal that the loop is ready for request submissions
            self.loop_ready.set()
            running = True
            while running:
                req = await request_queue.get()
                
                if req is None:
                    break

                # Clear any stale cancel state BEFORE dispatching the request.
                # This runs on the BLE thread, after the request is dequeued — so
                # it cannot race with cancel_current_operation() on the main thread
                # the way a reset inside scan_devices()/connect() would.
                myble.reset_cancel()

                try:
                    if req['topic'] == 'scan':
                        timeout, callback, filters,_ = parse(req, 5)
                        devices = await myble.scan_devices(timeout, filters)
                        if callback: callback(devices)
                        
                    elif req['topic'] == 'connect':
                        device, device_callback, connect_callback, disconnect_callback = parse(req)
                        success = await myble.connect(device, device_callback, disconnect_callback)
                        if connect_callback: connect_callback(success)
                        
                    elif req['topic'] == 'send':
                        device, message,_,_ = parse(req)
                        await myble.send(device, message)
                        
                    elif req['topic'] == 'disconnect':
                        device, _,_,_ = parse(req)
                        await myble.device_disconnect(device)
                        
                    elif req['topic'] == 'close_all':
                        myble.shutdown_all()
                        pending = getattr(myble, '_pending_disconnect_task', None)
                        if pending is not None:
                            try:
                                await asyncio.wait_for(pending, timeout=2.0)
                            except Exception:
                                logger.debug("Pending BLE disconnects did not complete in time", exc_info=True)
                    
                except Exception:
                    logger.exception("BLE worker loop error")
                    break

            logger.info("BLE worker loop completed")
            myble.shutdown_all()
            # Await any pending BLE disconnect coroutines that shutdown_all()
            # scheduled.  Without this the event loop exits before the
            # disconnect packet is sent and the remote device must wait for
            # the supervision timeout (~24 s) to notice the link loss.
            pending = getattr(myble, '_pending_disconnect_task', None)
            if pending is not None:
                try:
                    await asyncio.wait_for(pending, timeout=2.0)
                except Exception:
                    logger.debug("Pending BLE disconnects did not complete in time", exc_info=True)
            self._myble = None
            # Clear readiness if loop ends (allows restart scenarios)
            self.loop_ready.clear()

        logger.info("BLE worker thread starting")
        asyncio.run(worker_loop())
        logger.info("BLE worker thread finished")
