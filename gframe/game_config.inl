OPTION(irr::video::E_DRIVER_TYPE, driver_type, irr::video::EDT_COUNT)
#if (IRRLICHT_VERSION_MAJOR==1 && IRRLICHT_VERSION_MINOR==9)
#if EDOPRO_LINUX
OPTION_TAGGED(uint8_t, ygo::GameConfig::BoolMaybeUndefined, useWayland, 2)
#endif
#if EDOPRO_MACOS
OPTION_TAGGED(uint8_t, ygo::GameConfig::BoolMaybeUndefined, useIntegratedGpu, 2)
#endif
#endif
OPTION(uint8_t, vsync, 1)
OPTION_TAGGED(int, ygo::GameConfig::MaxFPSConfig, maxFPS, 60)
OPTION(bool, fullscreen, false)
OPTION(bool, showConsole, false)
OPTION(std::string, windowStruct, "")
OPTION(uint8_t, antialias, 0)
OPTION(uint32_t, coreLogOutput, ygo::CORE_LOG_TO_CHAT | ygo::CORE_LOG_TO_FILE)
OPTION(std::wstring, nickname, L"Player")
OPTION(std::wstring, gamename, L"Game")
OPTION(std::wstring, lastdeck, L"")
OPTION(uint32_t, lastlflist, 0)
OPTION(uint32_t, lastallowedcards, 3)
OPTION(uint64_t, lastDuelParam, 0x2E800) //#define DUEL_MODE_MR5
OPTION(uint32_t, lastExtraRules, 0)
OPTION(uint32_t, lastDuelForbidden, 0) //#define DUEL_MODE_MR5_FORB
OPTION(uint32_t, timeLimit, 180)
OPTION(uint32_t, team1count, 1)
OPTION(uint32_t, team2count, 1)
OPTION(uint32_t, bestOf, 1)
OPTION(uint32_t, startLP, 8000)
OPTION(uint32_t, startHand, 5)
OPTION(uint32_t, drawCount, 1)
OPTION(bool, relayDuel, false)
OPTION(bool, noShuffleDeck, false)
OPTION(bool, noCheckDeckContent, false)
OPTION(bool, noCheckDeckSize, false)
OPTION(bool, hideHandsInReplays, false)
OPTION(ygo::GameConfig::TextFont, textfont, EPRO_TEXT("fonts/NotoSansJP-Regular.otf"), 12)
OPTION(epro::path_string, numfont, EPRO_TEXT("fonts/NotoSansJP-Regular.otf"))
#ifdef YGOPRO_USE_BUNDLED_FONT
OPTION(ygo::GameConfig::FallbackFonts, fallbackFonts, ygo::GameConfig::TextFont{ epro::path_string{EPRO_TEXT("bundled")}, 12 })
#else
OPTION(ygo::GameConfig::FallbackFonts, fallbackFonts, )
#endif //YGOPRO_USE_BUNDLED_FONT
OPTION(std::wstring, serverport, L"7911")
OPTION(std::wstring, lasthost, L"127.0.0.1")
OPTION(std::wstring, lastport, L"7911")
OPTION(bool, botThrowRock, false)
OPTION(bool, botMute, false)
OPTION(int, lastBot, 0)
OPTION(std::wstring, lastServer, L"")
OPTION_ALIASED(bool, chkMAutoPos, automonsterpos, false)
OPTION_ALIASED(bool, chkSTAutoPos, autospellpos, false)
OPTION_ALIASED(bool, chkRandomPos, randompos, false)
OPTION_ALIASED(bool, chkAutoChain, autochain, false)
OPTION_ALIASED(bool, chkWaitChain, waitchain, false)
OPTION_ALIASED(bool, chkIgnore1, mute_opponent, false)
OPTION_ALIASED(bool, chkIgnore2, mute_spectators, false)
OPTION_ALIASED(bool, chkHideSetname, hide_setname, false)
OPTION_ALIASED(bool, chkHideHintButton, hide_hint_button, true)
OPTION_ALIASED(bool, chkAutoRPS, auto_rps, false)
OPTION(bool, draw_field_spell, true)
OPTION(bool, quick_animation, false)
OPTION(bool, alternative_phase_layout, false)
OPTION(bool, topdown_view, false)
OPTION(bool, keep_aspect_ratio, false)
OPTION(bool, keep_cardinfo_aspect_ratio, false)
OPTION(bool, showFPS, true)
OPTION(bool, hidePasscodeScope, false)
OPTION(bool, showScopeLabel, true)
OPTION(bool, ignoreDeckContents, false)
OPTION(bool, addCardNamesToDeckList, false)
OPTION(bool, filterBot, true)
OPTION_ALIASED(bool, chkAnime, show_unofficial, false)
#if EDOPRO_MACOS
OPTION(bool, ctrlClickIsRMB, true)
#else
OPTION(bool, ctrlClickIsRMB, false)
#endif
#if EDOPRO_ANDROID
OPTION(float, dpi_scale, 2.f)
#else
OPTION(float, dpi_scale, 1.f)
#endif
OPTION(epro::path_string, skin, EPRO_TEXT("none"))
OPTION(std::string, override_ssl_certificate_path, "")
OPTION_ALIASED(epro::path_string, locale, language, EPRO_TEXT("English"))
OPTION(bool, scale_background, true)
OPTION(bool, dotted_lines, false)
OPTION(bool, controller_input, false)
#if EDOPRO_ANDROID || EDOPRO_IOS
OPTION(bool, accurate_bg_resize, true)
OPTION(bool, confirm_clear_deck, true)
#else
OPTION(bool, accurate_bg_resize, false)
OPTION(bool, confirm_clear_deck, false)
#endif
OPTION_ALIASED(bool, enablemusic, enable_music, false)
OPTION_ALIASED(bool, enablesound, enable_sound, true)
OPTION_ALIASED_TAGGED(int, ygo::GameConfig::MusicConfig, musicVolume, music_volume, 20)
OPTION_ALIASED_TAGGED(int, ygo::GameConfig::MusicConfig, soundVolume, sound_volume, 20)
OPTION(bool, loopMusic, true)
OPTION(bool, saveHandTest, true)
OPTION(bool, discordIntegration, true)
OPTION(bool, noClientUpdates, false)
OPTION(bool, logDownloadErrors, false)
OPTION(uint8_t, maxImagesPerFrame, 50)
OPTION(uint8_t, imageLoadThreads, 4)
OPTION(uint8_t, imageDownloadThreads, 8)
OPTION(uint16_t, minMainDeckSize, 40)
OPTION(uint16_t, maxMainDeckSize, 60)
OPTION(uint16_t, minExtraDeckSize, 0)
OPTION(uint16_t, maxExtraDeckSize, 15)
OPTION(uint16_t, minSideDeckSize, 0)
OPTION(uint16_t, maxSideDeckSize, 15)
#if EDOPRO_ANDROID
OPTION(bool, native_keyboard, false)
OPTION(bool, native_mouse, false)
#endif
