package com.android.server.pm;

import static android.content.pm.UserInfo.FLAG_PROFILE;
import static android.os.UserManager.DISALLOW_AUTOFILL;
import static android.os.UserManager.DISALLOW_CONFIG_BRIGHTNESS;
import static android.os.UserManager.DISALLOW_CONFIG_DATE_TIME;
import static android.os.UserManager.USER_TYPE_FULL_DEMO;
import static android.os.UserManager.USER_TYPE_FULL_GUEST;
import static android.os.UserManager.USER_TYPE_FULL_RESTRICTED;
import static android.os.UserManager.USER_TYPE_FULL_SECONDARY;
import static android.os.UserManager.USER_TYPE_FULL_SYSTEM;
import static android.os.UserManager.USER_TYPE_PROFILE_CLONE;
import static android.os.UserManager.USER_TYPE_PROFILE_MANAGED;
import static android.os.UserManager.USER_TYPE_PROFILE_PRIVATE;

import static com.android.server.pm.UserRestrictionsFallbackInfo.FALLBACK_FROM_BASE_RESTRICTIONS;
import static com.android.server.pm.UserRestrictionsFallbackInfo.FALLBACK_FROM_EFFECTIVE_RESTRICTIONS;
import static com.android.server.pm.UserRestrictionsFallbackInfo.FALLBACK_FROM_PARENT_USER;

import android.app.ActivityManager;
import android.app.PropertyInvalidatedCache;
import android.content.Context;
import android.content.pm.UserInfo;
import android.content.res.Resources;
import android.os.Looper;
import android.os.UserManager;
import android.platform.test.annotations.Presubmit;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import com.android.server.LocalServices;

import com.google.common.truth.Expect;

import junitparams.JUnitParamsRunner;
import junitparams.Parameters;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

/**
 * Run with
 * {@code atest FrameworksServicesTests:com.android.server.pm.UserManagerServiceUserRestrictionsFallbackInfoTest}.
 */
@Presubmit
@MediumTest
@RunWith(JUnitParamsRunner.class)
@SuppressWarnings("deprecation")
public class UserManagerServiceUserRestrictionsFallbackInfoTest {

    @Rule
    public final Expect expect = Expect.create();

    private UserManagerService mUms;
    private final Context mContext = InstrumentationRegistry.getInstrumentation().getContext();
    private Resources mResources;

    @Before
    public void setup() throws Exception {
        // Currently UserManagerService cannot be instantiated twice inside a VM without a cleanup
        // TODO: Remove once UMS supports proper dependency injection
        if (Looper.myLooper() == null) {
            Looper.prepare();
        }
        // Disable binder caches in this process.
        PropertyInvalidatedCache.disableForTestMode();

        LocalServices.removeServiceForTest(UserManagerInternal.class);
        mUms = new UserManagerService(InstrumentationRegistry.getContext());
        // Put the current user to mUsers. UMS can't find userlist.xml, and fallbackToSingleUserLP.
        mUms.putUserInfo(
                new UserInfo(ActivityManager.getCurrentUser(), "Current User", 0));
        mResources = InstrumentationRegistry.getTargetContext().getResources();
    }

    @After
    public void tearDown() {
        LocalServices.removeServiceForTest(UserManagerInternal.class);
    }

    @Parameters({
            USER_TYPE_FULL_SYSTEM,
            USER_TYPE_FULL_SECONDARY,
            USER_TYPE_FULL_GUEST,
            USER_TYPE_FULL_RESTRICTED,
            USER_TYPE_FULL_DEMO
    })
    @Test
    @MediumTest
    public void testStandardFullUserTypesRestrictionsFallback_OnParentRequiredProfiles(String fullUserType) {
        int fullUserId = 300;
        UserInfo fullUser = createUserOfStandardType(fullUserType, fullUserId, fullUserId);
        mUms.putUserInfo(fullUser);
        UserInfo managedProfileAndDetails = createUserOfStandardType(
                USER_TYPE_PROFILE_MANAGED, fullUserId + 1, fullUserId);
        mUms.putUserInfo(managedProfileAndDetails);
        UserInfo cloneProfileAndDetails = createUserOfStandardType(
                USER_TYPE_PROFILE_CLONE, fullUserId + 2, fullUserId);
        mUms.putUserInfo(cloneProfileAndDetails);
        UserInfo privateProfileAndDetails = createUserOfStandardType(
                USER_TYPE_PROFILE_PRIVATE, fullUserId + 3, fullUserId);
        mUms.putUserInfo(privateProfileAndDetails);
        testRestrictionsFallbackForUsers(fullUser,
                List.of(managedProfileAndDetails, cloneProfileAndDetails, privateProfileAndDetails));
    }

    @Parameters({
            USER_TYPE_FULL_SYSTEM,
            USER_TYPE_FULL_SECONDARY,
            USER_TYPE_FULL_GUEST,
            USER_TYPE_FULL_RESTRICTED,
            USER_TYPE_FULL_DEMO
    })
    @Test
    @MediumTest
    public void testStandardFullUserTypesRestrictionsFallback_OnParentRequiredCustomProfiles(String fullUserType) {
        int fullUserId = 400;
        UserInfo fullUser = createUserOfStandardType(fullUserType, fullUserId, fullUserId);
        mUms.putUserInfo(fullUser);
        UserTypeDetails userTypeDetails1 = new UserTypeDetails.Builder()
                .setName("Profile 1")
                .setProfileParentRequired(true)
                .setBaseType(FLAG_PROFILE)
                .createUserTypeDetails();
        UserInfo profile1 = createUserFromDetails(
                "app.testing.profile.PROFILE_TYPE_1",
                userTypeDetails1, fullUserId + 1, fullUserId);
        UserTypeDetails userTypeDetails2 = new UserTypeDetails.Builder()
                .setName("Profile 2")
                .setProfileParentRequired(true)
                .setBaseType(FLAG_PROFILE)
                .createUserTypeDetails();
        UserInfo profile2 = createUserFromDetails(
                "app.testing.profile.PROFILE_TYPE_2",
                userTypeDetails2, fullUserId + 2, fullUserId);
        mUms.putUserInfo(profile1);
        testRestrictionsFallbackForUsers(fullUser, List.of(profile1, profile2));
    }

    private void testRestrictionsFallbackForUsers(UserInfo fullUser,
            List<UserInfo> profileInfoList) {
        for (UserInfo profileInfo: profileInfoList) {
            int fullUserId = fullUser.id;
            int profileUserId = profileInfo.id;
            for (String key: UserRestrictionsUtils.USER_RESTRICTIONS_FALLBACK_TO_PARENT) {
                try {
                    mUms.setUserRestriction(key, true, fullUserId);
                    expect.withMessage("hasUserRestriction(%s, %s)", key, profileUserId)
                            .that(mUms.hasUserRestriction(key, profileUserId)).isTrue();
                    expect.withMessage("hasBaseUserRestriction(%s, %s)", key, profileUserId)
                            .that(mUms.hasBaseUserRestriction(key, profileUserId)).isTrue();
                    List<UserManager.EnforcingUser> restrictionSources =
                            mUms.getUserRestrictionSources(key, profileUserId);
                    expect.withMessage("restrictionSources(%s, %s).size() == 1", key, profileUserId)
                            .that(restrictionSources.size()).isEqualTo(1);
                    expect.withMessage("restrictionSources(%s, %s) | "
                                    + "UserManager.RESTRICTION_SOURCE_SYSTEM "
                                    + "== UserManager.RESTRICTION_SOURCE_SYSTEM",
                                    key, profileUserId)
                            .that(restrictionSources.getFirst().getUserRestrictionSource() | UserManager.RESTRICTION_SOURCE_SYSTEM)
                            .isEqualTo(UserManager.RESTRICTION_SOURCE_SYSTEM);
                } finally {
                    mUms.setUserRestriction(key, false, fullUserId);
                }

                // This sets user restriction on device policy management restriction set.
                try {
                    mUms.setUserRestrictionInner(fullUserId, key, true);
                    expect.withMessage("hasUserRestriction(%s, %s)", key, profileUserId)
                            .that(mUms.hasUserRestriction(key, profileUserId)).isTrue();
                    List<UserManager.EnforcingUser> restrictionSources =
                            mUms.getUserRestrictionSources(key, profileUserId);
                    expect.withMessage("restrictionSources(%s, %s).size() == 0", key, profileUserId)
                            .that(restrictionSources.size()).isEqualTo(0);
                } finally {
                    mUms.setUserRestrictionInner(fullUserId, key, false);
                }
            }
        }
    }

    private UserInfo createUserOfStandardType(String userType, int userId, int profileGroupId) {
        UserTypeDetails userTypeDetails = UserTypeFactory.getUserTypes().get(userType);
        expect.withMessage("userTypeDetails for %s is non-null", userType)
                .that(userTypeDetails).isNotNull();
        return createUserFromDetails(userType, userTypeDetails, userId, profileGroupId);
    }

    private UserInfo createUserFromDetails(
            String userType, UserTypeDetails userTypeDetails, int userId, int profileGroupId) {
        int flags = userTypeDetails.getDefaultUserInfoFlags();
        return createUserOfType(userType, flags, userId, profileGroupId);
    }

    private UserInfo createUserOfType(String userType, int flags, int userId, int profileGroupId) {
        UserInfo userInfo = new UserInfo(userId, "A Name", "A path", flags, userType);
        userInfo.profileGroupId = profileGroupId;
        return userInfo;
    }
}
