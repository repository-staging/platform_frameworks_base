package com.android.server.pm;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.UserInfo;
import android.os.IpcDataCache;
import android.os.UserHandle;
import android.os.UserManager;
import android.platform.test.annotations.Postsubmit;
import android.util.ArraySet;
import android.util.Slog;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import static android.content.pm.UserInfo.*;
import static android.os.UserManager.*;

import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.fail;

/**
 * Run with
 * {@code atest FrameworksServicesTests:com.android.server.pm.UserManagerProfilesRestrictionTests}.
 * See {@link com.android.server.pm.UserManagerTest} for creating user testing references.
 */
@LargeTest
@Postsubmit
@RunWith(AndroidJUnit4.class)
    public class UserManagerProfilesRestrictionTests {
    // Taken from UserManagerService
    private static final long EPOCH_PLUS_30_YEARS = 30L * 365 * 24 * 60 * 60 * 1000L; // 30 years

    private static final int SWITCH_USER_TIMEOUT_SECONDS = 180; // 180 seconds
    private static final int REMOVE_USER_TIMEOUT_SECONDS = 180; // 180 seconds
    private static final String TAG = UserManagerProfilesRestrictionTests.class.getSimpleName();

    private final Context mContext = InstrumentationRegistry.getInstrumentation().getContext();

    private UserManager mUserManager = null;
    private ActivityManager mActivityManager;
    private PackageManager mPackageManager;
    private ArraySet<Integer> mUsersToRemove;
    private UserSwitchWaiter mUserSwitchWaiter;
    private UserRemovalWaiter mUserRemovalWaiter;
    private int mOriginalCurrentUserId;

    @Before
    public void setup() throws Exception {
        // Disable binder caches in this process.
        IpcDataCache.disableForTestMode();

        mOriginalCurrentUserId = ActivityManager.getCurrentUser();
        mUserManager = mContext.getSystemService(UserManager.class);
        mActivityManager = mContext.getSystemService(ActivityManager.class);
        mPackageManager = mContext.getPackageManager();
        mUserSwitchWaiter = new UserSwitchWaiter(TAG, SWITCH_USER_TIMEOUT_SECONDS);
        mUserRemovalWaiter = new UserRemovalWaiter(mContext, TAG, REMOVE_USER_TIMEOUT_SECONDS);

        mUsersToRemove = new ArraySet<>();
    }

    @After
    public void tearDown() throws Exception {
        if (mOriginalCurrentUserId != ActivityManager.getCurrentUser()) {
            switchUser(mOriginalCurrentUserId);
        }
        mUserSwitchWaiter.close();

        // Making a copy of mUsersToRemove to avoid ConcurrentModificationException
        mUsersToRemove.stream().toList().forEach(this::removeUser);
        mUserRemovalWaiter.close();
    }

    @Test
    @LargeTest
    public void testCreateManagedProfileOnMainUser_ThenTestFallbackRestrictions() {
        UserHandle mainUserHandle = mUserManager.getMainUser();
        UserInfo cloneProfile = createProfileForUser("SecondaryPrivate",
                USER_TYPE_PROFILE_MANAGED, mainUserHandle.getIdentifier());
        assertWithMessage("Private profile could not be created")
                .that(cloneProfile).isNotNull();

        testShouldFallback(DISALLOW_INSTALL_UNKNOWN_SOURCES, mainUserHandle, cloneProfile, true);
        testShouldFallback(DISALLOW_INSTALL_APPS, mainUserHandle, cloneProfile, true);
        testShouldFallback(DISALLOW_SET_USER_ICON, mainUserHandle, cloneProfile, false);
    }

    @Test
    @LargeTest
    public void testCreateCloneProfileOnMainUser_ThenTestFallbackRestrictions() {
        UserHandle mainUserHandle = mUserManager.getMainUser();
        UserInfo cloneProfile = createProfileForUser("SecondaryPrivate",
                USER_TYPE_PROFILE_CLONE, mainUserHandle.getIdentifier());
        assertWithMessage("Private profile could not be created")
                .that(cloneProfile).isNotNull();

        testShouldFallback(DISALLOW_INSTALL_UNKNOWN_SOURCES, mainUserHandle, cloneProfile, true);
        testShouldFallback(DISALLOW_INSTALL_APPS, mainUserHandle, cloneProfile, true);
        testShouldFallback(DISALLOW_SET_USER_ICON, mainUserHandle, cloneProfile, false);
    }

    @Test
    @LargeTest
    public void testCreatePrivateProfileOnMainUser_ThenTestFallbackRestrictions() {
        UserHandle mainUserHandle = mUserManager.getMainUser();
        UserInfo privateProfile = createProfileForUser("SecondaryPrivate",
                USER_TYPE_PROFILE_PRIVATE, mainUserHandle.getIdentifier());
        assertWithMessage("Private profile could not be created")
                .that(privateProfile).isNotNull();

        testShouldFallback(DISALLOW_INSTALL_UNKNOWN_SOURCES, mainUserHandle, privateProfile, true);
        testShouldFallback(DISALLOW_INSTALL_APPS, mainUserHandle, privateProfile, true);
        testShouldFallback(DISALLOW_SET_USER_ICON, mainUserHandle, privateProfile, false);
    }

    @Test
    @LargeTest
    public void testCreatePrivateProfileOnFullSecondaryUser_ThenTestFallbackRestrictions() {
        UserInfo secondaryUser = createUser("SecondaryWithPrivate",
                USER_TYPE_FULL_SECONDARY, FLAG_FULL);
        assertWithMessage("Secondary user could not be created")
                .that(secondaryUser).isNotNull();
        UserInfo privateProfile = createProfileForUser("SecondaryPrivate",
                USER_TYPE_PROFILE_PRIVATE, secondaryUser.id);
        assertWithMessage("Private profile could not be created")
                .that(privateProfile).isNotNull();

        testShouldFallback(DISALLOW_INSTALL_UNKNOWN_SOURCES, secondaryUser, privateProfile, true);
        testShouldFallback(DISALLOW_INSTALL_APPS, secondaryUser, privateProfile, true);
        testShouldFallback(DISALLOW_SET_USER_ICON, secondaryUser, privateProfile, false);
    }

    private void testShouldFallback(String restrictionKey,
            UserInfo userToSet, UserInfo userWithFallback, boolean shouldFallback) {
        UserHandle userToSetHandle = userToSet.getUserHandle();
        testShouldFallback(restrictionKey, userToSetHandle, userWithFallback, shouldFallback);
    }
    private void testShouldFallback(String restrictionKey,
            UserHandle userToSetHandle, UserInfo userWithFallback, boolean shouldFallback) {
        UserHandle userWithFallbackHandle = userWithFallback.getUserHandle();
        String fallbackUserType = userWithFallback.userType;
        mUserManager.setUserRestriction(restrictionKey, true, userToSetHandle);
        assertWithMessage("profile %s must %s have %s effective restriction",
                fallbackUserType, shouldFallback ? "not" : "", restrictionKey)
                .that(mUserManager.hasUserRestriction(restrictionKey, userWithFallbackHandle))
                .isEqualTo(shouldFallback);
        assertWithMessage("profile %s must %s have %s enabled on base restriction",
                fallbackUserType, shouldFallback ? "not" : "", restrictionKey)
                .that(mUserManager.hasBaseUserRestriction(restrictionKey, userWithFallbackHandle))
                .isEqualTo(shouldFallback);
        mUserManager.setUserRestriction(restrictionKey, false, userToSetHandle);
    }

    
    /**
     * See {@link com.android.server.pm.UserManagerTest#switchUser(int)}
     **/
    private void switchUser(int userId) {
        switchUserThenRun(userId, null);
    }

    /**
     * See {@link com.android.server.pm.UserManagerTest#switchUserThenRun(int, Runnable)}
     **/
    private void switchUserThenRun(int userId, Runnable runAfterSwitchBeforeWait) {
        Slog.d(TAG, "Switching to user " + userId);
        mUserSwitchWaiter.runThenWaitUntilSwitchCompleted(userId, () -> {
            // Start switching to user
            assertWithMessage("Could not start switching to user " + userId)
                    .that(mActivityManager.switchUser(userId)).isTrue();

            // While the user switch is happening, call runAfterSwitchBeforeWait.
            if (runAfterSwitchBeforeWait != null) {
                runAfterSwitchBeforeWait.run();
            }
        }, () -> fail("Could not complete switching to user " + userId));
    }

    /**
     * See {@link com.android.server.pm.UserManagerTest#removeUser(UserHandle)}
     **/
    private void removeUser(UserHandle userHandle) {
        mUserManager.removeUser(userHandle);
        waitForUserRemoval(userHandle.getIdentifier());
    }

    /**
     * See {@link com.android.server.pm.UserManagerTest#removeUser(int)}
     **/
    private void removeUser(int userId) {
        mUserManager.removeUser(userId);
        waitForUserRemoval(userId);
    }

    private void waitForUserRemoval(int userId) {
        mUserRemovalWaiter.waitFor(userId);
        mUsersToRemove.remove(userId);
    }

    /**
     * See {@link com.android.server.pm.UserManagerTest#createUser(String, String, int)}
     **/
    private UserInfo createUser(String name, String userType, int flags) {
        UserInfo user = mUserManager.createUser(name, userType, flags);
        if (user != null) {
            mUsersToRemove.add(user.id);
        }
        return user;
    }

    /**
     * See {@link com.android.server.pm.UserManagerTest#createProfileForUser(String, String, int)}
     **/
    private UserInfo createProfileForUser(String name, String userType, int userHandle) {
        return createProfileForUser(name, userType, userHandle, null);
    }

    /**
     * See {@link com.android.server.pm.UserManagerTest#createProfileForUser(String, String, int, String[])}
     **/
    private UserInfo createProfileForUser(String name, String userType, int userHandle,
            String[] disallowedPackages) {
        UserInfo profile = mUserManager.createProfileForUser(
                name, userType, 0, userHandle, disallowedPackages);
        if (profile != null) {
            mUsersToRemove.add(profile.id);
        }
        return profile;
    }

    /**
     * See {@link com.android.server.pm.UserManagerTest#createProfileEvenWhenDisallowedForUser(String, String, int)}
     **/
    private UserInfo createProfileEvenWhenDisallowedForUser(String name, String userType,
            int userHandle) {
        UserInfo profile = mUserManager.createProfileForUserEvenWhenDisallowed(
                name, userType, 0, userHandle, null);
        if (profile != null) {
            mUsersToRemove.add(profile.id);
        }
        return profile;
    }

    /**
     * See {@link com.android.server.pm.UserManagerTest#createRestrictedProfile(String)}
     **/
    private UserInfo createRestrictedProfile(String name) {
        UserInfo profile = mUserManager.createRestrictedProfile(name);
        if (profile != null) {
            mUsersToRemove.add(profile.id);
        }
        return profile;
    }
}
