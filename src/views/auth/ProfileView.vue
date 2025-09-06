<template>
  <q-page class="q-pa-md">
    <q-card>
      <q-card-section>
        <div class="text-h6">Edit Profile</div>
      </q-card-section>

      <q-card-section v-if="user">
        <q-form @submit.prevent="updateProfile">
          <q-input
            v-model="name"
            label="Name"
            :rules="[(val) => !!val || 'Name is required']"
          />

          <q-input v-model="user.email" label="Email" readonly />

          <div class="q-mt-md">
            <q-btn label="Save" type="submit" color="primary" />
          </div>
        </q-form>
      </q-card-section>

      <q-card-section v-else>
        <q-spinner />
        Loading profile...
      </q-card-section>
    </q-card>
  </q-page>
</template>

<script setup>
import { ref, onMounted } from "vue";
import { useAuthStore } from "@/stores/auth";
import { storeToRefs } from "pinia";

const authStore = useAuthStore();
const { user } = storeToRefs(authStore);

const name = ref("");

onMounted(() => {
  if (user.value) {
    name.value = user.value.name;
  }
});

const updateProfile = async () => {
  if (name.value && user.value) {
    await authStore.updateProfile({ id: user.value.id, name: name.value });
    // Optionally, show a notification on success
  }
};
</script>
