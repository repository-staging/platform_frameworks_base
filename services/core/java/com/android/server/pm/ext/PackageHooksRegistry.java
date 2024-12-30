package com.android.server.pm.ext;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.ext.PackageId;

import com.android.internal.pm.pkg.component.ParsedUsesPermissionImpl;
import com.android.internal.pm.pkg.parsing.PackageParsingHooks;

import java.util.List;

public class PackageHooksRegistry {

    public static PackageParsingHooks getParsingHooks(String pkgName) {
        PackageParsingHooks gmsCompatHooks = GmsCompatPkgParsingHooks.maybeGet(pkgName);
        if (gmsCompatHooks != null) {
            return gmsCompatHooks;
        }

        return switch (pkgName) {
            case PackageId.GSF_NAME -> new GsfParsingHooks();
            case PackageId.EUICC_SUPPORT_PIXEL_NAME -> new EuiccSupportPixelHooks.ParsingHooks();
            case PackageId.G_EUICC_LPA_NAME -> new EuiccGoogleHooks.ParsingHooks();
            case PackageId.PIXEL_CAMERA_SERVICES_NAME -> new PixelCameraServicesHooks.ParsingHooks();
            case PackageId.PIXEL_HEALTH_NAME -> new PixelHealthHooks.ParsingHooks();
            case "app.attestation.auditor" -> new PackageParsingHooks() {
                @NonNull
                @Override
                public List<ParsedUsesPermissionImpl> addUsesPermissions() {
                    return createUsesPerms("android.permission.READ_ADDITIONAL_SECURITY_STATE");
                }
            };
            default -> PackageParsingHooks.DEFAULT;
        };
    }

    public static PackageHooks getHooks(int packageId) {
        return switch (packageId) {
            case PackageId.EUICC_SUPPORT_PIXEL -> new EuiccSupportPixelHooks();
            case PackageId.G_CARRIER_SETTINGS -> new GCarrierSettingsHooks();
            case PackageId.G_EUICC_LPA -> new EuiccGoogleHooks();
            case PackageId.ANDROID_AUTO -> new AndroidAutoHooks();
            case PackageId.PIXEL_CAMERA_SERVICES -> new PixelCameraServicesHooks();
            case PackageId.PIXEL_HEALTH -> new PixelHealthHooks();
            default -> PackageHooks.DEFAULT;
        };
    }
}
