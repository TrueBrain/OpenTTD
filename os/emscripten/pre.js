Module.arguments.push('-mnull', '-snull', '-vsdl:relative_mode', '-c/user_data/openttd.cfg');

Module.preRun.push(function() {
    /* Because of the "-c" above, all user-data is stored in /user_data. */
    FS.mkdir('/user_data');
    FS.mount(IDBFS, {}, '/user_data');

    Module.addRunDependency("syncfs");
    FS.syncfs(true, function (err) {
        /* FS.mkdir() tends to fail if parent folders do not exist. */
        if (!FS.analyzePath("/user_data/content_download").exists) {
            FS.mkdir("/user_data/content_download");
        }
        if (!FS.analyzePath("/user_data/content_download/baseset").exists) {
            FS.mkdir("/user_data/content_download/baseset");
        }

        /* Check if the OpenGFX baseset is already downloaded. */
        if (!FS.analyzePath("/user_data/content_download/baseset/opengfx-0.6.0.tar").exists) {
            window.openttd_downloaded_opengfx = true;
            FS.createPreloadedFile("/user_data/content_download/baseset/", "opengfx-0.6.0.tar", "https://installer.cdn.openttd.org/emscripten/opengfx-0.6.0.tar", true, true);
        } else {
            /* Fake dependency increase, so the counter is stable. */
            Module.addRunDependency("opengfx");
            Module.removeRunDependency("opengfx");
        }

        Module.removeRunDependency("syncfs");
    });

    window.openttd_syncfs_shown_warning = false;
    window.openttd_syncfs = function() {
        /* Copy the virtual FS to the persistent storage. */
        FS.syncfs(false, function (err) { });

        /* On first time, warn the user about the volatile behaviour of
         * persistent storage. */
        if (!window.openttd_syncfs_shown_warning) {
            window.openttd_syncfs_shown_warning = true;
            Module.onWarningFs();
        }
    }

    window.openttd_exit = function() {
        Module.onExit();
    }
});

Module.postRun.push(function() {
    /* Check if we downloaded OpenGFX; if so, sync the virtual FS back to the
     * IDBFS so OpenGFX is stored persistent. */
    if (window["openttd_downloaded_opengfx"]) {
        FS.syncfs(false, function (err) { });
    }
});
