group = "app.pokemongo"

patches {
    about {
        name = "Pokémon GO RPC Probe"
        description = "Standalone diagnostic Morphe patch for Pokémon GO 0.431.0 ARM64"
        source = "https://github.com/belalsalah74/rpc"
        author = "Belal Salah"
        contact = "GitHub issues"
        website = "https://github.com/belalsalah74/rpc"
        license = "GPLv3"
    }
}

val patchListGeneratorClasspath = configurations.create("patchListGeneratorClasspath")

dependencies {
    compileOnly(libs.gson)
    patchListGeneratorClasspath(libs.gson)
}

tasks {
    register<JavaExec>("generatePatchesList") {
        description = "Build patch with patch list"
        dependsOn(build)
        classpath = sourceSets["main"].runtimeClasspath + patchListGeneratorClasspath
        mainClass.set("util.PatchListGeneratorKt")
    }
    publish {
        dependsOn("generatePatchesList")
    }
}
