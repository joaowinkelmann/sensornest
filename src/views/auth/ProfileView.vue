<template>
  <q-page class="q-pa-md">
    <q-card>
      <q-card-section>
        <div class="text-h6">{{ t('profile.title') }}</div>
      </q-card-section>

      <q-card-section v-if="user">
        <q-form @submit.prevent="updateProfile">
          <q-input
            v-model="name"
            :label="t('profile.nameLabel')"
            :rules="[(val) => !!val || t('profile.nameRequired')]"
          />

          <q-input v-model="user.email" :label="t('profile.emailLabel')" readonly />

          <div class="q-mt-md">
            <q-btn :label="t('profile.saveButton')" type="submit" color="primary" />
          </div>
        </q-form>
      </q-card-section>

      <q-card-section v-else>
        <q-spinner />
        {{ t('profile.loading') }}
      </q-card-section>
    </q-card>
  </q-page>
</template>

<script setup>
import { ref, onMounted, watch } from "vue";
import { useAuthStore } from "@/stores/auth";
import { storeToRefs } from "pinia";
import { useI18n } from "vue-i18n";

const { t } = useI18n();
const authStore = useAuthStore();
const { user } = storeToRefs(authStore);

const name = ref("");

watch(
  user,
  (newUser) => {
    if (newUser) {
      name.value = newUser.name || "";
    }
  },
  { immediate: true }
);

onMounted(async () => {
  // Ensure profile is loaded
  await authStore.getProfileIfNeeded({ force: true });
});

const updateProfile = async () => {
  if (name.value && user.value) {
    await authStore.updateProfile({ id: user.value.id, name: name.value });
  }
};
</script>
