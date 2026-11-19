plugins {
    id("com.android.application") version "8.5.1"
    id("org.jetbrains.kotlin.android") version "1.9.25"
}

android {
    namespace = "com.example.pickleballscoreboard"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.example.pickleballscoreboard"
        minSdk = 26
        targetSdk = 35
        versionCode = 2
        versionName = "1.1.0"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }

    kotlinOptions {
        jvmTarget = "1.8"
    }

    buildFeatures {
        compose = true
    }

    composeOptions {
        kotlinCompilerExtensionVersion = "1.5.15"
    }

    // Baseline records pre-existing lint issues (BLE permissions, dependency
    // versions) so lint passes; see lint-baseline.xml.
    lint {
        baseline = file("lint-baseline.xml")
    }

    // Share the glyph JSON files with the firmware (Spec 03 Section 6).
    sourceSets["main"].assets.srcDirs("../../shared/display_assets")
    sourceSets["test"].resources.srcDirs("../../shared/display_assets", "../../shared/render_fixtures", "../../shared/scoring_fixtures")
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2024.06.00"))
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.9.0")
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.2")
    debugImplementation("androidx.compose.ui:ui-tooling")
    debugImplementation("androidx.compose.ui:ui-test-manifest")
    androidTestImplementation(platform("androidx.compose:compose-bom:2024.06.00"))
    androidTestImplementation("androidx.compose.ui:ui-test-junit4")
    androidTestImplementation("androidx.test:runner:1.6.2")
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303") // actual asset parsing in pure pixel tests
}
