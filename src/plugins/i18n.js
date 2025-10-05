import { createI18n } from "vue-i18n";
import enUS from "@/locales/en-US.json";
import esES from "@/locales/es-ES.json";
import ptBR from "@/locales/pt-BR.json";

function getBrowserLocale() {
  const navigatorLocale = navigator.language || navigator.userLanguage;
  const supportedLocales = ["en-US", "es-ES", "pt-BR"];
  const lang = supportedLocales.find((l) =>
    navigatorLocale.startsWith(l.slice(0, 2)),
  );
  return lang || "en-US";
}

export default createI18n({
  legacy: false,
  locale: getBrowserLocale(),
  fallbackLocale: "en-US",
  messages: {
    "en-US": enUS,
    "es-ES": esES,
    "pt-BR": ptBR,
  },
});
