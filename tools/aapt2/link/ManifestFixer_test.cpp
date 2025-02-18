/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "link/ManifestFixer.h"

#include "test/Test.h"

using ::android::StringPiece;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::IsNull;
using ::testing::Ne;
using ::testing::NotNull;
using ::testing::StrEq;

namespace aapt {

struct ManifestFixerTest : public ::testing::Test {
  std::unique_ptr<IAaptContext> mContext;

  void SetUp() override {
    mContext =
        test::ContextBuilder()
            .SetCompilationPackage("android")
            .SetPackageId(0x01)
            .SetNameManglerPolicy(NameManglerPolicy{"android"})
            .AddSymbolSource(
                test::StaticSymbolSourceBuilder()
                    .AddSymbol(
                        "android:attr/package", ResourceId(0x01010000),
                        test::AttributeBuilder()
                            .SetTypeMask(android::ResTable_map::TYPE_STRING)
                            .Build())
                    .AddSymbol(
                        "android:attr/minSdkVersion", ResourceId(0x01010001),
                        test::AttributeBuilder()
                            .SetTypeMask(android::ResTable_map::TYPE_STRING |
                                         android::ResTable_map::TYPE_INTEGER)
                            .Build())
                    .AddSymbol(
                        "android:attr/targetSdkVersion", ResourceId(0x01010002),
                        test::AttributeBuilder()
                            .SetTypeMask(android::ResTable_map::TYPE_STRING |
                                         android::ResTable_map::TYPE_INTEGER)
                            .Build())
                    .AddSymbol("android:string/str", ResourceId(0x01060000))
                    .Build())
            .Build();
  }

  std::unique_ptr<xml::XmlResource> Verify(StringPiece str) {
    return VerifyWithOptions(str, {});
  }

  std::unique_ptr<xml::XmlResource> VerifyWithOptions(StringPiece str,
                                                      const ManifestFixerOptions& options) {
    std::unique_ptr<xml::XmlResource> doc = test::BuildXmlDom(str);
    ManifestFixer fixer(options);
    if (fixer.Consume(mContext.get(), doc.get())) {
      return doc;
    }
    return {};
  }
};

TEST_F(ManifestFixerTest, EnsureManifestIsRootTag) {
  EXPECT_THAT(Verify("<other-tag />"), IsNull());
  EXPECT_THAT(Verify("<ns:manifest xmlns:ns=\"com\" />"), IsNull());
  EXPECT_THAT(Verify("<manifest package=\"android\"></manifest>"), NotNull());
}

TEST_F(ManifestFixerTest, EnsureManifestHasPackage) {
  EXPECT_THAT(Verify("<manifest package=\"android\" />"), NotNull());
  EXPECT_THAT(Verify("<manifest package=\"com.android\" />"), NotNull());
  EXPECT_THAT(Verify("<manifest package=\"com.android.google\" />"), NotNull());
  EXPECT_THAT(Verify("<manifest package=\"com.android.google.Class$1\" />"), IsNull());
  EXPECT_THAT(Verify("<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" "
                     "android:package=\"com.android\" />"),
              IsNull());
  EXPECT_THAT(Verify("<manifest package=\"@string/str\" />"), IsNull());
}

TEST_F(ManifestFixerTest, AllowMetaData) {
  auto doc = Verify(R"EOF(
        <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                  package="android">
          <meta-data />
          <application>
            <meta-data />
            <activity android:name=".Hi"><meta-data /></activity>
            <activity-alias android:name=".Ho"><meta-data /></activity-alias>
            <receiver android:name=".OffTo"><meta-data /></receiver>
            <provider android:name=".Work"><meta-data /></provider>
            <service android:name=".We"><meta-data /></service>
          </application>
          <instrumentation android:name=".Go"><meta-data /></instrumentation>
        </manifest>)EOF");
  ASSERT_THAT(doc, NotNull());
}

TEST_F(ManifestFixerTest, UseDefaultSdkVersionsIfNonePresent) {
  ManifestFixerOptions options;
  options.min_sdk_version_default = std::string("8");
  options.target_sdk_version_default = std::string("22");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android">
        <uses-sdk android:minSdkVersion="7" android:targetSdkVersion="21" />
      </manifest>)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* el;
  xml::Attribute* attr;

  el = doc->root.get();
  ASSERT_THAT(el, NotNull());
  el = el->FindChild({}, "uses-sdk");
  ASSERT_THAT(el, NotNull());
  attr = el->FindAttribute(xml::kSchemaAndroid, "minSdkVersion");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("7"));
  attr = el->FindAttribute(xml::kSchemaAndroid, "targetSdkVersion");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("21"));

  doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android">
        <uses-sdk android:targetSdkVersion="21" />
      </manifest>)EOF",
                          options);
  ASSERT_THAT(doc, NotNull());

  el = doc->root.get();
  ASSERT_THAT(el, NotNull());
  el = el->FindChild({}, "uses-sdk");
  ASSERT_THAT(el, NotNull());
  attr = el->FindAttribute(xml::kSchemaAndroid, "minSdkVersion");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("8"));
  attr = el->FindAttribute(xml::kSchemaAndroid, "targetSdkVersion");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("21"));

  doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android">
        <uses-sdk />
      </manifest>)EOF",
                          options);
  ASSERT_THAT(doc, NotNull());

  el = doc->root.get();
  ASSERT_THAT(el, NotNull());
  el = el->FindChild({}, "uses-sdk");
  ASSERT_THAT(el, NotNull());
  attr = el->FindAttribute(xml::kSchemaAndroid, "minSdkVersion");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("8"));
  attr = el->FindAttribute(xml::kSchemaAndroid, "targetSdkVersion");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("22"));

  doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android" />)EOF",
                          options);
  ASSERT_THAT(doc, NotNull());

  el = doc->root.get();
  ASSERT_THAT(el, NotNull());
  el = el->FindChild({}, "uses-sdk");
  ASSERT_THAT(el, NotNull());
  attr = el->FindAttribute(xml::kSchemaAndroid, "minSdkVersion");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("8"));
  attr = el->FindAttribute(xml::kSchemaAndroid, "targetSdkVersion");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("22"));
}

TEST_F(ManifestFixerTest, UsesSdkMustComeBeforeApplication) {
  ManifestFixerOptions options;
  options.min_sdk_version_default = std::string("8");
  options.target_sdk_version_default = std::string("22");
  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
          <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                    package="android">
            <application android:name=".MainApplication" />
          </manifest>)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());
  ASSERT_EQ("manifest", manifest_el->name);

  xml::Element* application_el = manifest_el->FindChild("", "application");
  ASSERT_THAT(application_el, NotNull());

  xml::Element* uses_sdk_el = manifest_el->FindChild("", "uses-sdk");
  ASSERT_THAT(uses_sdk_el, NotNull());

  // Check that the uses_sdk_el comes before application_el in the children
  // vector.
  // Since there are no namespaces here, these children are direct descendants
  // of manifest.
  auto uses_sdk_iter =
      std::find_if(manifest_el->children.begin(), manifest_el->children.end(),
                   [&](const std::unique_ptr<xml::Node>& child) {
                     return child.get() == uses_sdk_el;
                   });

  auto application_iter =
      std::find_if(manifest_el->children.begin(), manifest_el->children.end(),
                   [&](const std::unique_ptr<xml::Node>& child) {
                     return child.get() == application_el;
                   });

  ASSERT_THAT(uses_sdk_iter, Ne(manifest_el->children.end()));
  ASSERT_THAT(application_iter, Ne(manifest_el->children.end()));

  // The distance should be positive, meaning uses_sdk_iter comes before
  // application_iter.
  EXPECT_THAT(std::distance(uses_sdk_iter, application_iter), Gt(0));
}

TEST_F(ManifestFixerTest, RenameManifestPackageAndFullyQualifyClasses) {
  ManifestFixerOptions options;
  options.rename_manifest_package = std::string("com.android");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android">
        <uses-split android:name="feature_a" />
        <application android:name=".MainApplication" text="hello">
          <activity android:name=".activity.Start" />
          <receiver android:name="com.google.android.Receiver" />
        </application>
      </manifest>)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr = nullptr;

  attr = manifest_el->FindAttribute({}, "package");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("com.android"));

  xml::Element* uses_split_el = manifest_el->FindChild({}, "uses-split");
  ASSERT_THAT(uses_split_el, NotNull());
  attr = uses_split_el->FindAttribute(xml::kSchemaAndroid, "name");
  ASSERT_THAT(attr, NotNull());
  // This should NOT have been affected.
  EXPECT_THAT(attr->value, StrEq("feature_a"));

  xml::Element* application_el = manifest_el->FindChild({}, "application");
  ASSERT_THAT(application_el, NotNull());

  attr = application_el->FindAttribute(xml::kSchemaAndroid, "name");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("android.MainApplication"));

  attr = application_el->FindAttribute({}, "text");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("hello"));

  xml::Element* el;
  el = application_el->FindChild({}, "activity");
  ASSERT_THAT(el, NotNull());

  attr = el->FindAttribute(xml::kSchemaAndroid, "name");
  ASSERT_THAT(el, NotNull());
  EXPECT_THAT(attr->value, StrEq("android.activity.Start"));

  el = application_el->FindChild({}, "receiver");
  ASSERT_THAT(el, NotNull());

  attr = el->FindAttribute(xml::kSchemaAndroid, "name");
  ASSERT_THAT(el, NotNull());
  EXPECT_THAT(attr->value, StrEq("com.google.android.Receiver"));
}

TEST_F(ManifestFixerTest,
       RenameManifestInstrumentationPackageAndFullyQualifyTarget) {
  ManifestFixerOptions options;
  options.rename_instrumentation_target_package = std::string("com.android");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android">
        <instrumentation android:name=".TestRunner" android:targetPackage="android" />
      </manifest>)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Element* instrumentation_el =
      manifest_el->FindChild({}, "instrumentation");
  ASSERT_THAT(instrumentation_el, NotNull());

  xml::Attribute* attr =
      instrumentation_el->FindAttribute(xml::kSchemaAndroid, "targetPackage");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("com.android"));
}

TEST_F(ManifestFixerTest,
       RenameManifestOverlayPackageAndFullyQualifyTarget) {
  ManifestFixerOptions options;
  options.rename_overlay_target_package = std::string("com.android");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android">
        <overlay android:targetName="Customization" android:targetPackage="android" />
      </manifest>)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Element* overlay_el =
      manifest_el->FindChild({}, "overlay");
  ASSERT_THAT(overlay_el, NotNull());

  xml::Attribute* attr =
      overlay_el->FindAttribute(xml::kSchemaAndroid, "targetPackage");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("com.android"));
}

TEST_F(ManifestFixerTest, AddOverlayCategory) {
  ManifestFixerOptions options;
  options.rename_overlay_category = std::string("category");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android">
        <overlay android:targetName="Customization" android:targetPackage="android" />
      </manifest>)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Element* overlay_el = manifest_el->FindChild({}, "overlay");
  ASSERT_THAT(overlay_el, NotNull());

  xml::Attribute* attr = overlay_el->FindAttribute(xml::kSchemaAndroid, "category");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("category"));
}

TEST_F(ManifestFixerTest, OverrideOverlayCategory) {
  ManifestFixerOptions options;
  options.rename_overlay_category = std::string("category");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android">
        <overlay android:targetName="Customization"
                 android:targetPackage="android"
                 android:category="yrogetac"/>
      </manifest>)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Element* overlay_el = manifest_el->FindChild({}, "overlay");
  ASSERT_THAT(overlay_el, NotNull());

  xml::Attribute* attr = overlay_el->FindAttribute(xml::kSchemaAndroid, "category");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("category"));
}

TEST_F(ManifestFixerTest, UseDefaultVersionNameAndCode) {
  ManifestFixerOptions options;
  options.version_name_default = std::string("Beta");
  options.version_code_default = std::string("0x10000000");
  options.version_code_major_default = std::string("0x20000000");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr =
      manifest_el->FindAttribute(xml::kSchemaAndroid, "versionName");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("Beta"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x10000000"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCodeMajor");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x20000000"));
}

TEST_F(ManifestFixerTest, DontUseDefaultVersionNameAndCode) {
  ManifestFixerOptions options;
  options.version_name_default = std::string("Beta");
  options.version_code_default = std::string("0x10000000");
  options.version_code_major_default = std::string("0x20000000");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
        <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                  package="android"
                  android:versionCode="0x00000001"
                  android:versionCodeMajor="0x00000002"
                  android:versionName="Alpha" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr =
      manifest_el->FindAttribute(xml::kSchemaAndroid, "versionName");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("Alpha"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x00000001"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCodeMajor");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x00000002"));
}

TEST_F(ManifestFixerTest, ReplaceVersionNameAndCode) {
  ManifestFixerOptions options;
  options.replace_version = true;
  options.version_name_default = std::string("Beta");
  options.version_code_default = std::string("0x10000000");
  options.version_code_major_default = std::string("0x20000000");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android"
                android:versionCode="0x00000001"
                android:versionCodeMajor="0x00000002"
                android:versionName="Alpha" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr =
      manifest_el->FindAttribute(xml::kSchemaAndroid, "versionName");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("Beta"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x10000000"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCodeMajor");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x20000000"));
}

TEST_F(ManifestFixerTest, UseDefaultRevisionCode) {
  ManifestFixerOptions options;
  options.revision_code_default = std::string("0x10000000");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android"
                android:versionCode="0x00000001" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "revisionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x10000000"));
}

TEST_F(ManifestFixerTest, DontUseDefaultRevisionCode) {
  ManifestFixerOptions options;
  options.revision_code_default = std::string("0x10000000");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
        <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                  package="android"
                  android:versionCode="0x00000001"
                  android:revisionCode="0x00000002" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "revisionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x00000002"));
}

TEST_F(ManifestFixerTest, ReplaceRevisionCode) {
  ManifestFixerOptions options;
  options.replace_version = true;
  options.revision_code_default = std::string("0x10000000");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
        <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                  package="android"
                  android:versionCode="0x00000001"
                  android:revisionCode="0x00000002" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "revisionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x10000000"));
}

TEST_F(ManifestFixerTest, ReplaceVersionName) {
  ManifestFixerOptions options;
  options.replace_version = true;
  options.version_name_default = std::string("Beta");


  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
              package="android"
              android:versionCode="0x00000001"
              android:versionCodeMajor="0x00000002"
              android:versionName="Alpha" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr =
      manifest_el->FindAttribute(xml::kSchemaAndroid, "versionName");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("Beta"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x00000001"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCodeMajor");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x00000002"));
}

TEST_F(ManifestFixerTest, ReplaceVersionCode) {
  ManifestFixerOptions options;
  options.replace_version = true;
  options.version_code_default = std::string("0x10000000");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
              package="android"
              android:versionCode="0x00000001"
              android:versionCodeMajor="0x00000002"
              android:versionName="Alpha" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr =
      manifest_el->FindAttribute(xml::kSchemaAndroid, "versionName");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("Alpha"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x10000000"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCodeMajor");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x00000002"));
}

TEST_F(ManifestFixerTest, ReplaceVersionCodeMajor) {
  ManifestFixerOptions options;
  options.replace_version = true;
  options.version_code_major_default = std::string("0x20000000");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
  <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android"
          android:versionCode="0x00000001"
          android:versionCodeMajor="0x00000002"
          android:versionName="Alpha" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr =
      manifest_el->FindAttribute(xml::kSchemaAndroid, "versionName");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("Alpha"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x00000001"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCodeMajor");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x20000000"));
}

TEST_F(ManifestFixerTest, DontReplaceVersionNameOrCode) {
  ManifestFixerOptions options;
  options.replace_version = true;

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
  <manifest xmlns:android="http://schemas.android.com/apk/res/android"
            package="android"
            android:versionCode="0x00000001"
            android:versionCodeMajor="0x00000002"
            android:versionName="Alpha" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr =
      manifest_el->FindAttribute(xml::kSchemaAndroid, "versionName");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("Alpha"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x00000001"));

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCodeMajor");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x00000002"));
}

TEST_F(ManifestFixerTest, MarkNonUpdatableSystem) {
  ManifestFixerOptions options;
  options.non_updatable_system = true;

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr = manifest_el->FindAttribute("", "updatableSystem");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("false"));
}

TEST_F(ManifestFixerTest, MarkNonUpdatableSystemOverwritingValue) {
  ManifestFixerOptions options;
  options.non_updatable_system = true;

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android"
                updatableSystem="true" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr = manifest_el->FindAttribute("", "updatableSystem");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("false"));
}

TEST_F(ManifestFixerTest, DontMarkNonUpdatableSystemWhenExplicitVersion) {
  ManifestFixerOptions options;
  options.non_updatable_system = true;

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android"
                android:versionCode="0x00000001" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr = manifest_el->FindAttribute("", "updatableSystem");
  ASSERT_THAT(attr, IsNull());
}

TEST_F(ManifestFixerTest, DontMarkNonUpdatableSystemWhenAddedVersion) {
  ManifestFixerOptions options;
  options.non_updatable_system = true;
  options.version_code_default = std::string("0x10000000");

  std::unique_ptr<xml::XmlResource> doc = VerifyWithOptions(R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android" />)EOF",
                                                            options);
  ASSERT_THAT(doc, NotNull());

  xml::Element* manifest_el = doc->root.get();
  ASSERT_THAT(manifest_el, NotNull());

  xml::Attribute* attr = manifest_el->FindAttribute("", "updatableSystem");
  ASSERT_THAT(attr, IsNull());

  attr = manifest_el->FindAttribute(xml::kSchemaAndroid, "versionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("0x10000000"));
}

TEST_F(ManifestFixerTest, EnsureManifestAttributesAreTyped) {
  EXPECT_THAT(Verify("<manifest package=\"android\" coreApp=\"hello\" />"), IsNull());
  EXPECT_THAT(Verify("<manifest package=\"android\" coreApp=\"1dp\" />"), IsNull());

  std::unique_ptr<xml::XmlResource> doc =
      Verify("<manifest package=\"android\" coreApp=\"true\" />");
  ASSERT_THAT(doc, NotNull());

  xml::Element* el = doc->root.get();
  ASSERT_THAT(el, NotNull());

  EXPECT_THAT(el->name, StrEq("manifest"));

  xml::Attribute* attr = el->FindAttribute("", "coreApp");
  ASSERT_THAT(attr, NotNull());

  EXPECT_THAT(attr->compiled_value, NotNull());
  EXPECT_THAT(ValueCast<BinaryPrimitive>(attr->compiled_value.get()), NotNull());
}

TEST_F(ManifestFixerTest, UsesFeatureMustHaveNameOrGlEsVersion) {
  std::string input = R"EOF(
        <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                  package="android">
          <uses-feature android:name="feature" />
          <uses-feature android:glEsVersion="1" />
          <feature-group />
          <feature-group>
            <uses-feature android:name="feature_in_group" />
            <uses-feature android:glEsVersion="2" />
          </feature-group>
        </manifest>)EOF";
  EXPECT_THAT(Verify(input), NotNull());

  input = R"EOF(
        <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                  package="android">
          <uses-feature android:name="feature" android:glEsVersion="1" />
        </manifest>)EOF";
  EXPECT_THAT(Verify(input), IsNull());

  input = R"EOF(
        <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                  package="android">
          <uses-feature />
        </manifest>)EOF";
  EXPECT_THAT(Verify(input), IsNull());

  input = R"EOF(
        <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                  package="android">
          <feature-group>
            <uses-feature android:name="feature" android:glEsVersion="1" />
          </feature-group>
        </manifest>)EOF";
  EXPECT_THAT(Verify(input), IsNull());

  input = R"EOF(
        <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                  package="android">
          <feature-group>
            <uses-feature />
          </feature-group>
        </manifest>)EOF";
  EXPECT_THAT(Verify(input), IsNull());
}

TEST_F(ManifestFixerTest, ApplicationInjectDebuggable) {
  ManifestFixerOptions options;
  options.debug_mode = true;

  std::string no_d = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
        </application>
      </manifest>)";

  std::string false_d = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application android:debuggable="false">
        </application>
      </manifest>)";

  std::string true_d = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application android:debuggable="true">
        </application>
      </manifest>)";

  // Inject the debuggable attribute when the attribute is not present and the
  // flag is present
  std::unique_ptr<xml::XmlResource> manifest = VerifyWithOptions(no_d, options);
  EXPECT_THAT(manifest->root.get()->FindChildWithAttribute(
      {}, "application", xml::kSchemaAndroid, "debuggable", "true"), NotNull());

  // Set the debuggable flag to true if the attribute is false and the flag is
  // present
  manifest = VerifyWithOptions(false_d, options);
  EXPECT_THAT(manifest->root.get()->FindChildWithAttribute(
      {}, "application", xml::kSchemaAndroid, "debuggable", "true"), NotNull());

  // Keep debuggable flag true if the attribute is true and the flag is present
  manifest = VerifyWithOptions(true_d, options);
  EXPECT_THAT(manifest->root.get()->FindChildWithAttribute(
      {}, "application", xml::kSchemaAndroid, "debuggable", "true"), NotNull());

  // Do not inject the debuggable attribute when the attribute is not present
  // and the flag is not present
  manifest = Verify(no_d);
  EXPECT_THAT(manifest->root.get()->FindChildWithAttribute(
      {}, "application", xml::kSchemaAndroid, "debuggable", "true"), IsNull());

  // Do not set the debuggable flag to true if the attribute is false and the
  // flag is not present
  manifest = Verify(false_d);
  EXPECT_THAT(manifest->root.get()->FindChildWithAttribute(
      {}, "application", xml::kSchemaAndroid, "debuggable", "true"), IsNull());

  // Keep debuggable flag true if the attribute is true and the flag is not
  // present
  manifest = Verify(true_d);
  EXPECT_THAT(manifest->root.get()->FindChildWithAttribute(
      {}, "application", xml::kSchemaAndroid, "debuggable", "true"), NotNull());
}

TEST_F(ManifestFixerTest, ApplicationProfileable) {
  std::string shell = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
          <profileable android:shell="true"/>
        </application>
      </manifest>)";
  EXPECT_THAT(Verify(shell), NotNull());
  std::string noshell = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
          <profileable/>
        </application>
      </manifest>)";
  EXPECT_THAT(Verify(noshell), NotNull());
}

TEST_F(ManifestFixerTest, IgnoreNamespacedElements) {
  std::string input = R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android">
        <special:tag whoo="true" xmlns:special="http://google.com" />
      </manifest>)EOF";
  EXPECT_THAT(Verify(input), NotNull());
}

TEST_F(ManifestFixerTest, DoNotIgnoreNonNamespacedElements) {
  std::string input = R"EOF(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
                package="android">
        <tag whoo="true" />
      </manifest>)EOF";
  EXPECT_THAT(Verify(input), IsNull());
}

TEST_F(ManifestFixerTest, SupportKeySets) {
  std::string input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <key-sets>
          <key-set android:name="old-set">
            <public-key android:name="old-key" android:value="some+old+key" />
          </key-set>
          <key-set android:name="new-set">
            <public-key android:name="new-key" android:value="some+new+key" />
          </key-set>
          <upgrade-key-set android:name="old-set" />
          <upgrade-key-set android:name="new-set" />
        </key-sets>
      </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());
}

TEST_F(ManifestFixerTest, InsertCompileSdkVersions) {
  std::string input = R"(<manifest package="com.pkg" />)";
  ManifestFixerOptions options;
  options.compile_sdk_version = {"28"};
  options.compile_sdk_version_codename = {"P"};

  std::unique_ptr<xml::XmlResource> manifest = VerifyWithOptions(input, options);
  ASSERT_THAT(manifest, NotNull());

  // There should be a declaration of kSchemaAndroid, even when the input
  // didn't have one.
  EXPECT_EQ(manifest->root->namespace_decls.size(), 1);
  EXPECT_EQ(manifest->root->namespace_decls[0].prefix, "android");
  EXPECT_EQ(manifest->root->namespace_decls[0].uri, xml::kSchemaAndroid);

  xml::Attribute* attr = manifest->root->FindAttribute(xml::kSchemaAndroid, "compileSdkVersion");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("28"));

  attr = manifest->root->FindAttribute(xml::kSchemaAndroid, "compileSdkVersionCodename");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("P"));

  attr = manifest->root->FindAttribute("", "platformBuildVersionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("28"));

  attr = manifest->root->FindAttribute("", "platformBuildVersionName");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("P"));
}

TEST_F(ManifestFixerTest, DoNotInsertCompileSdkVersions) {
  std::string input = R"(<manifest package="com.pkg" />)";
  ManifestFixerOptions options;
  options.no_compile_sdk_metadata = true;
  options.compile_sdk_version = {"28"};
  options.compile_sdk_version_codename = {"P"};

  std::unique_ptr<xml::XmlResource> manifest = VerifyWithOptions(input, options);
  ASSERT_THAT(manifest, NotNull());

  // There should be a declaration of kSchemaAndroid, even when the input
  // didn't have one.
  EXPECT_EQ(manifest->root->namespace_decls.size(), 1);
  EXPECT_EQ(manifest->root->namespace_decls[0].prefix, "android");
  EXPECT_EQ(manifest->root->namespace_decls[0].uri, xml::kSchemaAndroid);

  xml::Attribute* attr = manifest->root->FindAttribute(xml::kSchemaAndroid, "compileSdkVersion");
  ASSERT_THAT(attr, IsNull());

  attr = manifest->root->FindAttribute(xml::kSchemaAndroid, "compileSdkVersionCodename");
  ASSERT_THAT(attr, IsNull());

  attr = manifest->root->FindAttribute("", "platformBuildVersionCode");
  ASSERT_THAT(attr, IsNull());

  attr = manifest->root->FindAttribute("", "platformBuildVersionName");
  ASSERT_THAT(attr, IsNull());
}

TEST_F(ManifestFixerTest, OverrideCompileSdkVersions) {
  std::string input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android" package="android"
          compileSdkVersion="27" compileSdkVersionCodename="O"
          platformBuildVersionCode="27" platformBuildVersionName="O"/>)";
  ManifestFixerOptions options;
  options.compile_sdk_version = {"28"};
  options.compile_sdk_version_codename = {"P"};

  std::unique_ptr<xml::XmlResource> manifest = VerifyWithOptions(input, options);
  ASSERT_THAT(manifest, NotNull());

  xml::Attribute* attr = manifest->root->FindAttribute(xml::kSchemaAndroid, "compileSdkVersion");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("28"));

  attr = manifest->root->FindAttribute(xml::kSchemaAndroid, "compileSdkVersionCodename");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("P"));

  attr = manifest->root->FindAttribute("", "platformBuildVersionCode");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("28"));

  attr = manifest->root->FindAttribute("", "platformBuildVersionName");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("P"));
}

TEST_F(ManifestFixerTest, AndroidPrefixAlreadyUsed) {
  std::string input =
      R"(<manifest package="com.pkg"
         xmlns:android="http://schemas.android.com/apk/prv/res/android"
         android:private_attr="foo" />)";
  ManifestFixerOptions options;
  options.compile_sdk_version = {"28"};
  options.compile_sdk_version_codename = {"P"};

  std::unique_ptr<xml::XmlResource> manifest = VerifyWithOptions(input, options);
  ASSERT_THAT(manifest, NotNull());

  // Make sure that we don't redefine "android".
  EXPECT_EQ(manifest->root->namespace_decls.size(), 2);
  EXPECT_EQ(manifest->root->namespace_decls[0].prefix, "android");
  EXPECT_EQ(manifest->root->namespace_decls[0].uri,
            "http://schemas.android.com/apk/prv/res/android");
  EXPECT_EQ(manifest->root->namespace_decls[1].prefix, "android0");
  EXPECT_EQ(manifest->root->namespace_decls[1].uri, xml::kSchemaAndroid);
}

TEST_F(ManifestFixerTest, UnexpectedElementsInManifest) {
  std::string input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <beep/>
      </manifest>)";
  ManifestFixerOptions options;
  options.warn_validation = true;

  // Unexpected element should result in a warning if the flag is set to 'true'.
  std::unique_ptr<xml::XmlResource> manifest = VerifyWithOptions(input, options);
  ASSERT_THAT(manifest, NotNull());

  // Unexpected element should result in an error if the flag is set to 'false'.
  options.warn_validation = false;
  manifest = VerifyWithOptions(input, options);
  ASSERT_THAT(manifest, IsNull());

  // By default the flag should be set to 'false'.
  manifest = Verify(input);
  ASSERT_THAT(manifest, IsNull());
}

TEST_F(ManifestFixerTest, InsertFingerprintPrefixIfNotExist) {
  std::string input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
      </manifest>)";
  ManifestFixerOptions options;
  options.fingerprint_prefixes = {"foo", "bar"};

  std::unique_ptr<xml::XmlResource> manifest = VerifyWithOptions(input, options);
  ASSERT_THAT(manifest, NotNull());
  xml::Element* install_constraints = manifest->root.get()->FindChild({}, "install-constraints");
  ASSERT_THAT(install_constraints, NotNull());
  std::vector<xml::Element*> fingerprint_prefixes = install_constraints->GetChildElements();
  EXPECT_EQ(fingerprint_prefixes.size(), 2);
  xml::Attribute* attr;
  EXPECT_THAT(fingerprint_prefixes[0]->name, StrEq("fingerprint-prefix"));
  attr = fingerprint_prefixes[0]->FindAttribute(xml::kSchemaAndroid, "value");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("foo"));
  EXPECT_THAT(fingerprint_prefixes[1]->name, StrEq("fingerprint-prefix"));
  attr = fingerprint_prefixes[1]->FindAttribute(xml::kSchemaAndroid, "value");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("bar"));
}

TEST_F(ManifestFixerTest, AppendFingerprintPrefixIfExists) {
  std::string input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
          <install-constraints>
            <fingerprint-prefix android:value="foo" />
          </install-constraints>
      </manifest>)";
  ManifestFixerOptions options;
  options.fingerprint_prefixes = {"bar", "baz"};

  std::unique_ptr<xml::XmlResource> manifest = VerifyWithOptions(input, options);
  ASSERT_THAT(manifest, NotNull());
  xml::Element* install_constraints = manifest->root.get()->FindChild({}, "install-constraints");
  ASSERT_THAT(install_constraints, NotNull());
  std::vector<xml::Element*> fingerprint_prefixes = install_constraints->GetChildElements();
  EXPECT_EQ(fingerprint_prefixes.size(), 3);
  xml::Attribute* attr;
  EXPECT_THAT(fingerprint_prefixes[0]->name, StrEq("fingerprint-prefix"));
  attr = fingerprint_prefixes[0]->FindAttribute(xml::kSchemaAndroid, "value");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("foo"));
  EXPECT_THAT(fingerprint_prefixes[1]->name, StrEq("fingerprint-prefix"));
  attr = fingerprint_prefixes[1]->FindAttribute(xml::kSchemaAndroid, "value");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("bar"));
  EXPECT_THAT(fingerprint_prefixes[2]->name, StrEq("fingerprint-prefix"));
  attr = fingerprint_prefixes[2]->FindAttribute(xml::kSchemaAndroid, "value");
  ASSERT_THAT(attr, NotNull());
  EXPECT_THAT(attr->value, StrEq("baz"));
}

TEST_F(ManifestFixerTest, UsesLibraryMustHaveNonEmptyName) {
  std::string input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
          <uses-library android:name="" />
        </application>
      </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
          <uses-library />
        </application>
      </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  input = R"(
       <manifest xmlns:android="http://schemas.android.com/apk/res/android"
           package="android">
         <application>
           <uses-library android:name="blahhh" />
         </application>
       </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());
}

TEST_F(ManifestFixerTest, ApplicationPropertyAttributeRequired) {
  std::string input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
          <property android:name="" />
        </application>
      </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());
}

TEST_F(ManifestFixerTest, ApplicationPropertyOnlyOneAttributeDefined) {
  std::string input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
          <property android:name="" android:value="" android:resource="" />
        </application>
      </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
          <property android:name="" android:resource="" />
        </application>
      </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
          <property android:name="" android:value="" />
        </application>
      </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());
}

TEST_F(ManifestFixerTest, ComponentPropertyOnlyOneAttributeDefined) {
  std::string input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
          <activity android:name=".MyActivity">
            <property android:name="" android:value="" android:resource="" />
          </activity>
        </application>
      </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
          <activity android:name=".MyActivity">
            <property android:name="" android:value="" />
          </activity>
        </application>
      </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  input = R"(
      <manifest xmlns:android="http://schemas.android.com/apk/res/android"
          package="android">
        <application>
          <activity android:name=".MyActivity">
            <property android:name="" android:resource="" />
          </activity>
        </application>
      </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());
}

TEST_F(ManifestFixerTest, IntentFilterActionMustHaveNonEmptyName) {
  std::string input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
             package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.MAIN" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());
}

TEST_F(ManifestFixerTest, IntentFilterCategoryMustHaveNonEmptyName) {
  std::string input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <category android:name="" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
             package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <category />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <category android:name="android.intent.category.LAUNCHER" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());
}

TEST_F(ManifestFixerTest, IntentFilterPathMustStartWithLeadingSlashOnDeepLinks) {
  // No DeepLink.
  std::string input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
             package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <data />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  // No DeepLink, missing ACTION_VIEW.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:pathPrefix="pathPattern" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  // DeepLink, missing DEFAULT category while DEFAULT is recommended but not required.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:pathPrefix="pathPattern" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  // No DeepLink, missing BROWSABLE category.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:pathPrefix="pathPattern" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  // No DeepLink, missing 'android:scheme' in <data> tag.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:host="www.example.com"
                          android:pathPrefix="pathPattern" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  // No DeepLink, <action> is ACTION_MAIN not ACTION_VIEW.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.MAIN" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:pathPrefix="pathPattern" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  // DeepLink with no leading slash in android:path.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:path="path" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  // DeepLink with leading slash in android:path.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:path="/path" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  // DeepLink with no leading slash in android:pathPrefix.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:pathPrefix="pathPrefix" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  // DeepLink with leading slash in android:pathPrefix.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:pathPrefix="/pathPrefix" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  // DeepLink with no leading slash in android:pathPattern.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:pathPattern="pathPattern" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), IsNull());

  // DeepLink with leading slash in android:pathPattern.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:pathPattern="/pathPattern" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  // DeepLink with '.' start in pathPattern.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:pathPattern=".*\\.pathPattern" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  // DeepLink with '*' start in pathPattern.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:pathPattern="*" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());

  // DeepLink with string reference as a path.
  input = R"(
    <manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="android">
      <application>
        <activity android:name=".MainActivity">
          <intent-filter>
            <action android:name="android.intent.action.VIEW" />
            <category android:name="android.intent.category.DEFAULT" />
            <category android:name="android.intent.category.BROWSABLE" />
            <data android:scheme="http"
                          android:host="www.example.com"
                          android:path="@string/startup_uri" />
          </intent-filter>
        </activity>
      </application>
    </manifest>)";
  EXPECT_THAT(Verify(input), NotNull());
}
}  // namespace aapt
