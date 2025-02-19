**Note:**
This bug was present since at least firefox-17.0 (2012) and has been (mostly) fixed since firefox-120.0 and firefox-115.5esr,
see [CVE-2023-6208](https://www.mozilla.org/en-US/security/advisories/mfsa2023-49/#CVE-2023-6208).<br>
Other gecko-based browsers (e.g. seamonkey-2.53.19 as of 2024-10-14) may still be vulnerable.

---

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

Browsers like firefox have the concepts of "secure context" (e.g. `https://`)
and "[transient user activation]"; the javascript from the page gets some
temporary powers as soon as you have interacted *even so little* with the
page, like clicked, touched, etc.

For instance, writing with `Clipboard.writeText()` to the windows-style
Ctrl-C Ctrl-V *clipboard* selection is only possible from secure contexts
and only in the short while after the user has clicked a button, etc on the page.
As this bug demonstrates, those prerequisites are not needed for writing to the
*primary* selection, which on X11 is much more used and much more valuable.

[transient user activation]: https://developer.mozilla.org/en-US/docs/Web/Security/User_activation

### Workaround

Without patching firefox, the only workaround I can think of is
catching the `Selection.selectAllChildren()` function from an addon's
content script, and either block it or reimplement it via other function
like `addRange()`, e.g.:

	let block = function(){ throw Error('blocked') };
	exportFunction(block, Selection.prototype, { defineAs: 'selectAllChildren' });

[Complete extension on addons.mozilla.org][amo-xpi]. If you're running firefox-esr, the developer
edition or nightly, you can just set `xpinstall.signatures.required` to `true` in
`about:config`, get the xpi from [here][xpi] and install it with `firefox no-sel.xpi`.
Notice that blocking `selectAllChildren()` may break some sites, example [here][quote-reply].

[xpi]: no-sel.xpi
[amo-xpi]: https://addons.mozilla.org/en-US/firefox/addon/disable-the-selection-api/
[quote-reply]: https://github.com/turistu/odds-n-ends/issues/2

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
<sub>[nsFrameSelection.cpp:AutoCopyListener::OnSelectionChange()](https://github.com/mozilla/gecko-dev/blob/aa189da432fac500242d9374f044947357f7144e/layout/generic/nsFrameSelection.cpp#L3326)</sub>

The idea of this patch was to *always* prevent javascript from indirectly
messing with the primary selection via the Selection API. However, it does
not live up to that promise, because the `JS_REASON` flag is not reliable; if
javascript calls some function like `addRange()` or `selectAllChildren()`
while the user has started dragging but hasn't released the mouse button yet,
that code will be called *without* that flag but with the text set by javascript,
not the text selected by the user. However, this is still enough to prevent
javascript from setting the selection while the user is not interacting with
the page at all.

**Update:** a virtually identical [patch] was added to mozilla-central on Oct 26 2023,
but without referencing or addressing the issue mentioned in the paragraph above. The
patch has made its way since then into the firefox-120.0b5 developer/beta version.

[patch]: https://hg.mozilla.org/mozilla-central/rev/88e0043c5aa4234dada941ac2fd0ded875210508

### About the example and bracketed-paste

The bracketed paste feature of bash/readline and zsh means that you
cannot just append a `\r` or `\n` to the payload and be done, it's the
user who has to press ENTER for it to run.

However, workarounds exist. As [noticed by David Leadbeater][ctrl-c], in the
default configurations of some terminals like xterm[^xterm], just inserting a `^C`
in the payload is enough to bypass the bracketed-paste mode of shells like
bash, zsh or fish:

        writeXPrimary('\u0003;touch ~/LOL-' + Date.now() / 1000 +'\r')

Other terminals like mlterm don't filter out the pasted data at all, and you
can terminate the pasting mode early on with a `\e[201~` escape.

For bash, you can take also advantage of some quirks in the readline library
to turn off the highlighting and make the payload invisible to the user [^bash].
E.g. ([live example here][bash-pastejack]):

	let payload = 'touch ~/LOL-' + Date.now() / 1000;
	writeXPrimary('\n' + payload + '\n'.repeat(100) + ' '.repeat(30)
		+ '\n'.repeat(100))

which will confuse the user with the same screen as when some stray background job
had written something to the terminal:

	user@host:~$ : previous unrelated command
	user@host:~$	<-- paste here
	#   <-- cursor here, most users will just hit Enter to get a new prompt

[ctrl-c]: https://www.openwall.com/lists/oss-security/2023/10/20/2

[bash-pastejack]:	https://turistu.github.io/firefox/bash-pastejack.html

[^bash]: this issue was fixed in the devel branch of bash by commit [ad39c5];
the code is still marked [experimental][display.c] and not yet included in any release, though.

[ad39c5]: https://git.savannah.gnu.org/cgit/bash.git/commit/?h=devel&id=ad39c5
[display.c]: https://git.savannah.gnu.org/cgit/bash.git/tree/lib/readline/display.c?h=devel&id=fe24a6a55e8850298b496c5b9d82f1866eba190e#n1315

Just to be clear, I don't think that either mlterm, bash, nor the shells that
don't do have that bracketed-paste feature are at fault here in any way
(and I personally always turn off that misfeature as it badly interferes
with my workflow): It's firefox which should get all the blame for letting
random javascript evade its pretended "sandbox" in this way.

### About Wayland

For firefox running in Wayland, `writeXPrimary()` will only succeed
when the firefox window (the main window, not necessarily the tab the code
runs in) has the focus. Otherwise the selection will be cleared. At first I
assumed that this is something specific to the Wayland protocol, but that
turned out to be utterly false; it's just some quirk, bug or "feature"
specific to either firefox itself or GTK.

But I think that's still bad enough, even if the page should take care to
only set the selection when the main window has the keyboard focus.

And of course, all this doesn't affect the situation where you're copying
and pasting in another firefox tab with a different context, origin, etc.

### About the timeout

Repeating the script faster than each half second or couple of seconds
would be counterproductive, as it would cause the load to increase, the
carets and screen to flicker and the user to immediately notice that she
cannot select anything (as most X11 programs --but firefox-- remove the
text highlighting when they lose the primary selection ownership).

Yet should the attacker have some special reason to bypass the
`setInterval()` ~1 second or so "throttling" when the tab is in background,
she can do it with a `data:` iframe that
[reloads itself via meta-refresh][meta-refresh]. This assumes that the
victim hasn't turned `accessibility.blockautorefresh` on in `about:config`.

[meta-refresh]: iframe-meta-refresh.md

### A more general problem and solution

Even if the Selection API functions were fixed to only set the primary
selection when called from a secure context and as a result of user action,
the fact that they do it as a hidden side-effect means that they would still
be quite broken because:
- they end up changing the primary selection [when not intended either by
the author or the user][quote-reply],
- they cannot be reliably used by legitimate scripts, because
they don't give any indication on whether setting the primary selection
has succeeded or not.

My proposal is to completely remove any changing of primary selection as
a side-effect of using the javascript Selection API (i.e. do a more robust
patch than the one above), and then add an option to the
[Clipboard API][clipboard-api] functions to use the primary instead of
the clipboard selection, subject to the same restrictions which currently
apply (secure context and user activation). Example usage:
```
navigator.clipboard.writeText(text, {useX11Primary: true})
```
Such a thing is trivial to implement, and will be beneficial to both
authors (who could make online editors and similar scripts actually usable
on X11, which is rarely --if ever-- the case now) and to end users who could
easily block it from an addon's content script or from the site's
permissions without breaking other functionality.

[clipboard-api]: https://w3c.github.io/clipboard-apis/#clipboard-interface

[^xterm]: the defaults were changed (since this write-up was published) in
[xterm-388](https://invisible-island.net/xterm/xterm.log.html#xterm_388).
