package com.android.server.ext;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;

import com.android.internal.os.BackgroundThread;
import com.android.internal.os.SELinuxFlags;
import com.android.server.pm.PackageManagerService;

public final class SystemServerExt {

    public final Context context;
    public final Handler bgHandler;
    public final PackageManagerService packageManager;

    private SystemServerExt(Context systemContext, PackageManagerService pm) {
        context = systemContext;
        bgHandler = BackgroundThread.getHandler();
        packageManager = pm;
    }

    /*
     Called after system server has completed its initialization,
     but before any of the apps are started.

     Call from com.android.server.SystemServer#startOtherServices(), at the end of lambda
     that is passed into mActivityManagerService.systemReady()
     */
    public static void init(Context systemContext, PackageManagerService pm) {
        SystemServerExt sse = new SystemServerExt(systemContext, pm);
        sse.bgHandler.post(sse::initBgThread);

        AppCompatConf.init(systemContext);
    }

    void initBgThread() {
        WifiAutoOff.maybeInit(this);
        BluetoothAutoOff.maybeInit(this);

        if (Build.IS_DEBUGGABLE) {
            if (!SELinuxFlags.kernelSupportsSELinuxFlags()) {
                String title = "Kernel doesn't support SELinux flags";
                String msg = "App hardening features that use SELinux flags, such as DCL and ptrace restrictions, do not work.";
                new SystemErrorNotification("missing hardening", title, msg).show(context);
            }
        }
    }
}
