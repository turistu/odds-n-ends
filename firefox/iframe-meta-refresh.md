When a tab is in the background, firefox throttles some javascript functions like `setInterval()` and `requestAnimationFrame()` (and css animations), by spacing out the intervals at which the callbacks are run.

If you want to bypass that throttling, you can use a `data:` iframe that reloads
itself via meta-refresh with a zero value:
```
<iframe style=display:none src='data:text/html,
	<meta http-equiv=refresh content=0>
	<script>dump("hallo baby "+Date.now()/1000+"\n")</script>
'></iframe>
```
Turn on `browser.dom.window.dump.enabled` in `about:config` to see the
effect.

To prevent this from happening to you, set `accessibility.blockautorefresh`
in `about:config` (that will unfortunately break a lot of stupid sites).

Notice that --because of leaks and bugs-- the tab will sooner or later eat
all its vm space, and finally crash and burn ;-)
```
...
[unhandlable oom] Failed to mmap, likely no more mappings available /builds/worker/checkouts/gecko/memory/build/mozjemalloc.cpp : 1705[Parent 73963, IPC I/O Parent] WARNING: process 74077 exited on signal 11: file /builds/worker/checkouts/gecko/ipc/chromium/src/base/process_util_posix.cc:265
```
But with the current 118 I was still able to keep it going at 100% CPU for
almost 5 minutes on my machine, much more than expected.

If you want to use a callback in the parent window, `setInterval()`-wise,
you can easily do it with `postMessage` / `onmessage`:
```
<script>
let i = 0;
onmessage = function(){
	dump(`here you go ${i++}\n`);
}
</script>
<iframe style=display:none src='data:text/html,
	<meta http-equiv=refresh content=0>
	<script>parent.postMessage("", "*")</script>
'></iframe>
```
