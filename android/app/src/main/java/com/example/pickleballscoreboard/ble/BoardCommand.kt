package com.example.pickleballscoreboard.ble

// Spec 02 §4: each command is one exact ASCII write.
enum class BoardCommand(
    val wire: String,
) {
    LeftPlus("+L"),
    LeftMinus("-L"),
    RightPlus("+R"),
    RightMinus("-R"),
    NextServe("S"),
    SwitchCourts("C"),
    EndGame("E"),
    Undo("U"),
    NewDoublesGame("0D"),
    NewSinglesGame("0S"),
    ;

    val bytes: ByteArray get() = wire.toByteArray(Charsets.US_ASCII)
}
