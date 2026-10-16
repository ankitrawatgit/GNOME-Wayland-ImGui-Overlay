import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

const APP_ID = 'io.github.ankitrawat.GnomeImGuiOverlay';
const WINDOW_TITLE = 'GNOME ImGui Overlay';

export default class OverlayController extends Extension {
    enable() {
        this._shownSignals = new Map();
        this._windowCreatedId = global.display.connect(
            'window-created',
            (_display, window) => this._watchNewWindow(window));

        // Handle an overlay that was already open when the extension loaded.
        for (const actor of global.get_window_actors()) {
            const window = actor.metaWindow;
            if (this._matches(window))
                this._manage(window);
        }
    }

    disable() {
        if (this._windowCreatedId) {
            global.display.disconnect(this._windowCreatedId);
            this._windowCreatedId = 0;
        }

        for (const [window, signalId] of this._shownSignals) {
            try {
                window.disconnect(signalId);
            } catch {
                // The window may already have been destroyed.
            }
        }
        this._shownSignals.clear();
        this._shownSignals = null;
    }

    _watchNewWindow(window) {
        // SDL sends the Wayland app-id and title before the first frame. The
        // compositor's "shown" signal therefore gives us an event-driven point
        // at which both identity and initial geometry are ready.
        const signalId = window.connect('shown', () => {
            window.disconnect(signalId);
            this._shownSignals?.delete(window);
            if (this._matches(window))
                this._manage(window);
        });
        this._shownSignals.set(window, signalId);
    }

    _matches(window) {
        return window.get_gtk_application_id?.() === APP_ID ||
            window.get_wm_class?.() === APP_ID ||
            window.get_title?.() === WINDOW_TITLE;
    }

    _manage(window) {
        if (window.is_fullscreen())
            window.unmake_fullscreen();
        if (window.is_maximized())
            window.unmaximize();

        const monitorIndex = window.get_monitor();
        const monitor = Main.layoutManager.monitors[monitorIndex] ??
            Main.layoutManager.primaryMonitor;
        if (!monitor)
            return;

        window.move_resize_frame(
            false, monitor.x, monitor.y, monitor.width, monitor.height);
        window.make_above();
    }
}
