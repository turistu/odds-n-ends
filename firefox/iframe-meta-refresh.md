When a tab is in the background, most browsers throttle javascript functions like `setInterval()` or `requestAnimationFrame()` (and css animations), by spacing out the intervals at which the callbacks are run.

If you want to bypass that throttling, you can use a `data:` iframe that reloads
itself via meta-refresh with a zero value:
```
<iframe style=display:none src='data:text/html,
	<!doctype html>
	<meta http-equiv=refresh content=0>
	<script>parent.postMessage("", "*")</script>
'></iframe>
<script>
onmessage = function(){
	// your code here
}
```
[Live example with some crude stats](https://turistu.github.io/firefox/meta-refresh.html)

In firefox, you can prevent this from being inflicted upon you by setting
`accessibility.blockautorefresh` to `true` in `about:config` (but that will
unfortunately break a lot of stupid sites).

Also, just pressing the stop button (or `window.stop()` from javascript)
will kill this madness too -- but, of course, the javascript from the page
can easily restart it any time it likes.

Notice that in firefox --because of leaks and bugs-- the tab will sooner
or later eat all its vm space, and finally crash and burn:
```
...
[unhandlable oom] Failed to mmap, likely no more mappings available /builds/worker/checkouts/gecko/memory/build/mozjemalloc.cpp : 1705[Parent 73963, IPC I/O Parent] WARNING: process 74077 exited on signal 11: file /builds/worker/checkouts/gecko/ipc/chromium/src/base/process_util_posix.cc:265
```
But with the current firefox-118 I was still able to keep it going at 100% CPU
for almost 5 minutes on my machine, more than expected ;-)
