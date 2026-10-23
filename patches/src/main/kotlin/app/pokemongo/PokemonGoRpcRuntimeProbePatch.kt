package app.pokemongo

import app.morphe.patcher.patch.AppTarget
import app.morphe.patcher.patch.Compatibility
import app.morphe.patcher.patch.rawResourcePatch

private val COMPATIBILITY_POKEMON_GO_QUICK_CATCH = Compatibility(
    name = "Pokémon GO",
    packageName = "com.nianticlabs.pokemongo",
    targets = listOf(AppTarget(version = "0.433.1")),
)

/**
 * Quick Catch and Excellent curve-throw routines adapted from PGRazor for the pinned
 * Pokémon GO 0.433.1 ARM64 build. The read-only RPC probe remains available for diagnostics.
 */
@Suppress("unused")
val pokemonGoQuickCatchEnhancedThrow = rawResourcePatch(
    name = "Quick Catch + Enhanced Throw",
    description = "Accelerated catch presentation, automatic post-catch dismissal, and Excellent curve throws for Pokémon GO 0.433.1 ARM64. Includes read-only RPC diagnostics.",
    default = false,
) {
    compatibleWith(COMPATIBILITY_POKEMON_GO_QUICK_CATCH)
    execute {
        val nativeDir = get("lib/arm64-v8a")
        val original = nativeDir.resolve("libmain.so")
        val backup = nativeDir.resolve("libmain_orig.so")
        require(original.exists()) { "lib/arm64-v8a/libmain.so not found" }
        require(!backup.exists()) { "libmain_orig.so already exists; patch a clean, official Pokémon GO APK" }
        val loader = object {}.javaClass.classLoader ?: error("Unable to resolve patch bundle class loader")
        fun resource(name: String) = loader.getResourceAsStream(name)?.use { it.readBytes() }
            ?: error("Missing resource $name")
        val bootstrap = resource("pgo/arm64-v8a/libmain.so")
        val probe = resource("pgo/arm64-v8a/libpgo_rpc_probe.so")
        val assets = mapOf(
            "libfrida-gadget.so" to resource("pgo/arm64-v8a/libfrida-gadget.so"),
            "agent.js" to resource("pgo/quickcatch/agent.js"),
            "quick-ball.js" to resource("pgo/quickcatch/quick-ball.js"),
            "quick-close.js" to resource("pgo/quickcatch/quick-close.js"),
            "catch-presentation-bridge.js" to resource("pgo/quickcatch/catch-presentation-bridge.js"),
            "PGRazor-LICENSE.txt" to resource("pgo/quickcatch/PGRazor-LICENSE.txt"),
            "FRIDA-COPYING.txt" to resource("pgo/quickcatch/FRIDA-COPYING.txt"),
            "FRIDA-NOTICE.txt" to resource("pgo/quickcatch/FRIDA-NOTICE.txt"),
        )
        val packagedAssets = get("assets/pgorpc")
        require(packagedAssets.isDirectory || packagedAssets.mkdirs()) { "Unable to create assets/pgorpc in the APK" }
        assets.forEach { (name, content) -> packagedAssets.resolve(name).writeBytes(content) }
        original.copyTo(backup, overwrite = false)
        original.writeBytes(bootstrap)
        nativeDir.resolve("libpgo_rpc_probe.so").writeBytes(probe)
    }
}
