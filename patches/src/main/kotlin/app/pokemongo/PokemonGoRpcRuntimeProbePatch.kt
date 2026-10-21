package app.pokemongo

import app.morphe.patcher.patch.AppTarget
import app.morphe.patcher.patch.Compatibility
import app.morphe.patcher.patch.rawResourcePatch

private val COMPATIBILITY_POKEMON_GO_RPC_PROBE = Compatibility(
    name = "Pokémon GO",
    packageName = "com.nianticlabs.pokemongo",
    targets = listOf(
        AppTarget(version = "0.433.1"),
        AppTarget(version = "0.431.0"),
    ),
)

/**
 * Read-only runtime metadata diagnostics for Pokémon GO 0.433.1 / 0.431.0 ARM64.
 * This does not invoke managed methods or send game RPC requests.
 */
@Suppress("unused")
val pokemonGoRpcRuntimeProbe = rawResourcePatch(
    name = "RPC runtime probe",
    description = "Logs selected IL2CPP RPC method signatures, code RVAs, field offsets and references for Pokémon GO 0.433.1 / 0.431.0 ARM64. Output goes to logcat (PGORPCPROBE) and files/rpc_probe.txt.",
    default = false,
) {
    compatibleWith(COMPATIBILITY_POKEMON_GO_RPC_PROBE)

    execute {
        val dir = get("lib/arm64-v8a")
        val original = dir.resolve("libmain.so")
        val backup = dir.resolve("libmain_orig.so")
        require(original.exists()) { "lib/arm64-v8a/libmain.so not found" }
        require(!backup.exists()) {
            "libmain_orig.so already exists; patch an unmodified official Pokémon GO APK"
        }

        val classLoader = object {}.javaClass.classLoader
            ?: error("Unable to resolve patch bundle class loader")
        fun resource(name: String) = classLoader.getResourceAsStream(name)?.use { it.readBytes() }
            ?: error("Missing resource $name")

        val bootstrap = resource("pgo/arm64-v8a/libmain.so")
        val probe = resource("pgo/arm64-v8a/libpgo_rpc_probe.so")

        original.copyTo(backup, overwrite = false)
        original.writeBytes(bootstrap)
        dir.resolve("libpgo_rpc_probe.so").writeBytes(probe)
    }
}
