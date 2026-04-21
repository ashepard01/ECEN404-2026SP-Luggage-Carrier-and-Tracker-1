// ----------------------------------------------------
// Rover Tracker – Login + Navigation + Full Map Logic
// ----------------------------------------------------

import React, { useState, useEffect, useRef, useMemo } from "react";
import { View, Text, TouchableOpacity, StyleSheet, Alert, SafeAreaView, Animated, Easing, ScrollView,} from "react-native";
import MapView, { Marker, AnimatedRegion, Polyline } from "react-native-maps";
import { Ionicons, MaterialCommunityIcons } from "@expo/vector-icons";
import * as Location from "expo-location";
import * as Notifications from "expo-notifications";
import { TextInput } from "react-native"; 
// Firebase auth imports
import { initializeApp, getApps, getApp } from "firebase/app";
import { getAuth, signInWithEmailAndPassword, createUserWithEmailAndPassword } from "firebase/auth";
// Firebase Firestore imports
import {
  getFirestore,
  doc,
  setDoc,
  addDoc,
  updateDoc,
  collection,
  serverTimestamp,
  query,
  where,
  orderBy,
  getDocs,
} from "firebase/firestore";


// Navigation Imports
import { NavigationContainer } from "@react-navigation/native";
import { createNativeStackNavigator } from "@react-navigation/native-stack";

// Firebase config
const firebaseConfig = {
  apiKey: "AIzaSyBV6yt7GSb-0LUaFhdUUeoRMo9U9NV7MHE",
  authDomain: "find-my-rover.firebaseapp.com",
  projectId: "find-my-rover",
  storageBucket: "find-my-rover.firebasestorage.app",
  messagingSenderId: "824971402088",
  appId: "1:824971402088:web:54a3ec2191b4fbb3d494dd",
  measurementId: "G-9EYE537VXL",
};

// Nav stack + shared state
const Stack = createNativeStackNavigator();
const ProfileContext = React.createContext();

// Init Firebase once (avoid duplicate init on fast refresh)
const firebaseApp = getApps().length ? getApp() : initializeApp(firebaseConfig);
const auth = getAuth(firebaseApp);
const db = getFirestore(firebaseApp);

// Show local notifications while app is active
Notifications.setNotificationHandler({
  handleNotification: async () => ({
    shouldShowBanner: true,
    shouldShowList: true,
    shouldPlaySound: true,
    shouldSetBadge: false,
  }),
});


// ----------------------------------------------------
// Login Screen 
// ----------------------------------------------------
function LoginScreen({ navigation }) {
  // Profile state for auth info
  const { setProfile } = React.useContext(ProfileContext);
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  // Block repeat login taps
  const [isLoggingIn, setIsLoggingIn] = useState(false);

  // Firebase login
  const handleFirebaseLogin = async () => {
    if (isLoggingIn) return;
    if (!email || !password) {
      Alert.alert("Login Required", "Enter email & password.");
      return;
    }
    setIsLoggingIn(true);
    try {
      // Sign in with Firebase
      const cred = await signInWithEmailAndPassword(auth, email, password);
      // Save auth email into profile
      setProfile((prev) => ({
        ...prev,
        email: cred?.user?.email || email,
        lastLogin: new Date().toLocaleString(),
      }));
      // Upsert user profile in Firestore
      await setDoc(
        doc(db, "users", cred.user.uid),
        {
          userId: cred.user.uid,
          email: cred?.user?.email || email,
          updatedAt: serverTimestamp(),
        },
        { merge: true }
      );
      // Go to map and let map screen show the welcome popup
      navigation.replace("Map", { showWelcome: true });
    } catch (err) {
      Alert.alert("Login Failed", err?.message || "Unable to sign in.");
    } finally {
      // Re-enable login button after request ends
      setIsLoggingIn(false);
    }
  };

  // Login screen layout
  return (
    <SafeAreaView style={styles.loginContainer}>
      <MaterialCommunityIcons name="robot" size={100} color="white" />
      <Text style={styles.loginTitle}>ROVER TRACKER</Text>

      <View style={styles.loginBox}>
        <Text style={styles.loginLabel}>Email</Text>
        {/* Email input */}
        <TextInput
          style={styles.inputBox}
          value={email}
          onChangeText={setEmail}
          placeholder="Enter email"
          placeholderTextColor="#ddd"
          autoCapitalize="none"
          keyboardType="email-address"
        />

        <Text style={styles.loginLabel}>Password</Text>
        {/* Password input */}
        <TextInput
          style={styles.inputBox}
          value={password}
          onChangeText={setPassword}
          placeholder="Enter password"
          placeholderTextColor="#ddd"
          secureTextEntry
        />
        {/* Login button */}
        <TouchableOpacity
          style={[styles.loginButton, isLoggingIn && styles.disabledButton]}
          onPress={handleFirebaseLogin}
          disabled={isLoggingIn}
        >
          <Text style={styles.loginButtonText}>Login</Text>
        </TouchableOpacity>
        {/* Go to sign up */}
        <TouchableOpacity
          style={styles.signupLink}
          onPress={() => navigation.navigate("SignUp")}
        >
          <Text style={styles.signupLinkText}>Create Account</Text>
        </TouchableOpacity>
      </View>
    </SafeAreaView>
  );
}

// ----------------------------------------------------
// SIGN UP SCREEN
// ----------------------------------------------------
function SignUpScreen({ navigation }) {
  // Profile state for new user
  const { setProfile } = React.useContext(ProfileContext);
  // Name field for profile
  const [name, setName] = useState("");
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  // Block repeat sign-up taps
  const [isSigningUp, setIsSigningUp] = useState(false);

  // Simple password rule check
  const isPasswordValid = (pwd) => {
    const hasMinLen = pwd.length >= 8;
    const hasUpper = /[A-Z]/.test(pwd);
    const hasNumber = /[0-9]/.test(pwd);
    const hasSpecial = /[^A-Za-z0-9]/.test(pwd);
    return hasMinLen && hasUpper && hasNumber && hasSpecial;
  };

  // Firebase sign up
  const handleSignUp = async () => {
    if (isSigningUp) return;
    if (!email || !password) {
      Alert.alert("Sign Up Required", "Enter email & password.");
      return;
    }
    if (!isPasswordValid(password)) {
      Alert.alert(
        "Weak Password",
        "Password must be 8+ chars with 1 capital, 1 number, 1 special."
      );
      return;
    }
    setIsSigningUp(true);
    try {
      // Create user with Firebase
      const cred = await createUserWithEmailAndPassword(auth, email, password);
      // Save name + auth email into profile
      setProfile((prev) => ({
        ...prev,
        name: name || prev.name,
        email: cred?.user?.email || email,
        lastLogin: new Date().toLocaleString(),
      }));
      // Upsert user profile in Firestore
      await setDoc(
        doc(db, "users", cred.user.uid),
        {
          userId: cred.user.uid,
          name: name || "",
          email: cred?.user?.email || email,
          createdAt: serverTimestamp(),
          updatedAt: serverTimestamp(),
        },
        { merge: true }
      );
      // Go to map and let map screen show the welcome popup
      navigation.replace("Map", { showWelcome: true });
    } catch (err) {
      Alert.alert("Sign Up Failed", err?.message || "Unable to create account.");
    } finally {
      // Re-enable sign-up button after request ends
      setIsSigningUp(false);
    }
  };

  // Sign up layout
  return (
    <SafeAreaView style={styles.loginContainer}>
      <MaterialCommunityIcons name="robot" size={90} color="white" />
      <Text style={styles.loginTitle}>CREATE ACCOUNT</Text>

      <View style={styles.loginBox}>
        <Text style={styles.loginLabel}>Name</Text>
        {/* Name input */}
        <TextInput
          style={styles.inputBox}
          value={name}
          onChangeText={setName}
          placeholder="Enter name"
          placeholderTextColor="#ddd"
        />

        <Text style={styles.loginLabel}>Email</Text>
        {/* Email input */}
        <TextInput
          style={styles.inputBox}
          value={email}
          onChangeText={setEmail}
          placeholder="Enter email"
          placeholderTextColor="#ddd"
          autoCapitalize="none"
          keyboardType="email-address"
        />

        <Text style={styles.loginLabel}>Password</Text>
        {/* Password input */}
        <TextInput
          style={styles.inputBox}
          value={password}
          onChangeText={setPassword}
          placeholder="Enter password"
          placeholderTextColor="#ddd"
          secureTextEntry
        />

        {/* Sign up button */}
        <TouchableOpacity
          style={[styles.loginButton, isSigningUp && styles.disabledButton]}
          onPress={handleSignUp}
          disabled={isSigningUp}
        >
          <Text style={styles.loginButtonText}>Sign Up</Text>
        </TouchableOpacity>

        {/* Back to login */}
        <TouchableOpacity
          style={styles.signupLink}
          onPress={() => navigation.goBack()}
        >
          <Text style={styles.signupLinkText}>Back to Login</Text>
        </TouchableOpacity>
      </View>
    </SafeAreaView>
  );
}
// ----------------------------------------------------
// USER PROFILE SCREEN
// ----------------------------------------------------
function ProfileScreen({ navigation }) {
  const { profile, userPath, roverPath, setUserPath, setRoverPath, setPendingLogout } = React.useContext(ProfileContext);
  // Default map view if no path yet
  const fallbackRegion = {
    latitude: 30.6211667,
    longitude: -96.3403889,
    latitudeDelta: 0.0015,
    longitudeDelta: 0.0015,
  };
  const lastUserPoint = userPath[userPath.length - 1];
  const lastRoverPoint = roverPath[roverPath.length - 1];
  // Center the preview on the latest point
  const initialRegion = lastUserPoint || lastRoverPoint
    ? {
        latitude: (lastUserPoint?.latitude ?? lastRoverPoint.latitude),
        longitude: (lastUserPoint?.longitude ?? lastRoverPoint.longitude),
        latitudeDelta: 0.0015,
        longitudeDelta: 0.0015,
      }
    : fallbackRegion;

  return (
    <SafeAreaView style={styles.profileContainer}>
      
      {/* ---- Profile Card ---- */}
      <View style={styles.profileCardFixed}>
        
        <Text style={styles.profileLabelFixed}>Name:</Text>
        <Text style={styles.profileValueFixed}>{profile.name}</Text>

        <Text style={styles.profileLabelFixed}>Email:</Text>
        <Text style={styles.profileValueFixed}>{profile.email || "—"}</Text>

        <Text style={styles.profileLabelFixed}>Last Login:</Text>
        <Text style={styles.profileValueFixed}>{profile.lastLogin}</Text>

      </View>

      {/* ---- Path Preview ---- */}
      <View style={styles.profileMapContainer}>
        <MapView style={styles.profileMap} initialRegion={initialRegion}>
          {roverPath.length > 1 && (
            <Polyline
              coordinates={roverPath}
              strokeColor="#e74c3c"
              strokeWidth={3}
            />
          )}
          {userPath.length > 1 && (
            <Polyline
              coordinates={userPath}
              strokeColor="#3498db"
              strokeWidth={3}
            />
          )}
        </MapView>
      </View>

      {/* ---- Edit Profile Button ---- */}
      <TouchableOpacity
        style={styles.editProfileButtonFixed}
        onPress={() => navigation.navigate("EditProfile")}
      >
        <Ionicons name="create-outline" size={18} color="white" />
        <Text style={styles.editProfileTextFixed}>Edit Profile</Text>
      </TouchableOpacity>

      {/* ---- Back Button ---- */}
      <TouchableOpacity
        style={styles.profileBackButtonFixed}
        onPress={() => navigation.navigate("Map")}
      >
        <Ionicons name="arrow-back" size={20} color="white" />
        <Text style={styles.profileBackTextFixed}>Back to Map</Text>
      </TouchableOpacity>
      {/* ---- History Button ---- */}
      <TouchableOpacity
        style={styles.profileHistoryButton}
        onPress={() => navigation.navigate("History")}
      >
        <Ionicons name="time-outline" size={20} color="white" />
        <Text style={styles.profileBackTextFixed}>Session History</Text>
      </TouchableOpacity>

      {/* ---- Sign Out (Bottom Right) ---- */}
      <TouchableOpacity
        style={styles.profileSignOutButtonFixed}
        onPress={() => {
          // Mark logout so active session can be finalized
          setPendingLogout(true);
          setUserPath([]);
          setRoverPath([]);
          navigation.replace("Login");
        }}
      >
        <Ionicons name="log-out-outline" size={20} color="white" />
        <Text style={styles.profileSignOutTextFixed}>Sign Out</Text>
      </TouchableOpacity>

    </SafeAreaView>
  );
}

// Edit profile screen + save
function EditProfileScreen({ navigation }) {
  const { profile, setProfile } = React.useContext(ProfileContext);

  const [name, setName] = useState(profile.name);
  const [email, setEmail] = useState(profile.email);

  // Save edits back into profile state
  const saveChanges = () => {
    setProfile({
      ...profile,
      name,
      email
    });

    Alert.alert("Saved", "Your profile has been updated.");
    navigation.goBack();
  };

  return (
    <SafeAreaView style={styles.editContainer}>
      <Text style={styles.editTitle}>Edit Profile</Text>

      <View style={styles.editBox}>
        <Text style={styles.editLabel}>Name</Text>
        <TextInput
          style={styles.editInputBox}
          value={name}
          onChangeText={setName}
          placeholder="Enter name"
          placeholderTextColor="#ccc"
        />
      </View>

      <TouchableOpacity style={styles.saveButton} onPress={saveChanges}>
        <Text style={styles.saveButtonText}>Save Changes</Text>
      </TouchableOpacity>
    </SafeAreaView>
  );
}

// ----------------------------------------------------
// HISTORY SCREEN
// ----------------------------------------------------
function HistoryScreen({ navigation }) {
  const [sessions, setSessions] = useState([]);
  const [loading, setLoading] = useState(true);

  // Load session list for current user
  useEffect(() => {
    const loadSessions = async () => {
      try {
        const user = auth.currentUser;
        if (!user) {
          setSessions([]);
          return;
        }
        // Query by userId (current schema)
        const qUserId = query(
          collection(db, "sessions"),
          where("userId", "==", user.uid)
        );
        // Query by userID (legacy/case variant)
        const qUserID = query(
          collection(db, "sessions"),
          where("userID", "==", user.uid)
        );
        const [snapUserId, snapUserID] = await Promise.all([getDocs(qUserId), getDocs(qUserID)]);
        // Merge results from both field variants
        const map = new Map();
        snapUserId.docs.forEach((d) => map.set(d.id, { id: d.id, ...d.data() }));
        snapUserID.docs.forEach((d) => map.set(d.id, { id: d.id, ...d.data() }));
        const rows = Array.from(map.values());
        // Sort newest first in app (avoids composite index requirement)
        rows.sort((a, b) => {
          const aMs = a?.startTime?.toMillis ? a.startTime.toMillis() : 0;
          const bMs = b?.startTime?.toMillis ? b.startTime.toMillis() : 0;
          return bMs - aMs;
        });
        setSessions(rows);
      } catch (err) {
        Alert.alert("History Error", err?.message || "Unable to load sessions.");
      } finally {
        setLoading(false);
      }
    };
    loadSessions();
  }, []);

  // One row in history list
  const renderSession = (s) => {
    // Build labels from start + logout times
    const startDateObj = s?.startTime?.toDate ? s.startTime.toDate() : null;
    const logoutDateObj = s?.endTime?.toDate ? s.endTime.toDate() : null;
    const startDateLabel = startDateObj ? startDateObj.toLocaleDateString() : "No date";
    const startTimeLabel = startDateObj ? startDateObj.toLocaleTimeString() : "No start time";
    const logoutTimeLabel = logoutDateObj ? logoutDateObj.toLocaleTimeString() : "Active";
    return (
      <TouchableOpacity
        key={s.id}
        style={styles.historyRow}
        onPress={() => navigation.navigate("Playback", { sessionId: s.id })}
      >
        <Text style={styles.historyTitle}>{startDateLabel} - Logout {logoutTimeLabel}</Text>
        <Text style={styles.historyText}>Start: {startTimeLabel}</Text>
        <Text style={styles.historyText}>Mode: {s.startMode || "—"}</Text>
        <Text style={styles.historyText}>Status: {s.status || "—"}</Text>
      </TouchableOpacity>
    );
  };

  return (
    <SafeAreaView style={styles.profileContainer}>
      <Text style={styles.editTitle}>Session History</Text>
      <View style={styles.historyList}>
        {loading ? (
          <Text style={styles.historyText}>Loading...</Text>
        ) : sessions.length === 0 ? (
          <Text style={styles.historyText}>No sessions yet.</Text>
        ) : (
          // Scrollable list of sessions
          <ScrollView
            style={styles.historyScroll}
            contentContainerStyle={styles.historyListContent}
            showsVerticalScrollIndicator={true}
          >
            {sessions.map(renderSession)}
          </ScrollView>
        )}
      </View>
      <TouchableOpacity
        style={styles.profileBackButtonFixed}
        onPress={() => navigation.goBack()}
      >
        <Ionicons name="arrow-back" size={20} color="white" />
        <Text style={styles.profileBackTextFixed}>Back</Text>
      </TouchableOpacity>
    </SafeAreaView>
  );
}

// ----------------------------------------------------
// PLAYBACK SCREEN
// ----------------------------------------------------
function PlaybackScreen({ route, navigation }) {
  const { sessionId } = route.params || {};
  const [points, setPoints] = useState([]);
  const [playIdx, setPlayIdx] = useState(0);

  // Load points for selected session
  useEffect(() => {
    const loadPoints = async () => {
      try {
        if (!sessionId) return;
        const q = query(
          collection(db, "sessions", sessionId, "points"),
          orderBy("clientTs", "asc")
        );
        const snap = await getDocs(q);
        const rows = snap.docs.map((d) => ({
          id: d.id,
          lat: d.data().lat,
          lon: d.data().lon,
          pointType: d.data().pointType || "rover",
        }));
        setPoints(rows);
      } catch (err) {
        Alert.alert("Playback Error", err?.message || "Unable to load points.");
      }
    };
    loadPoints();
  }, [sessionId]);

  // Move marker along points for simple playback
  useEffect(() => {
    if (points.length < 2) return;
    setPlayIdx(0);
    const maxFrames = points.length;
    const id = setInterval(() => {
      setPlayIdx((prev) => {
        if (prev >= maxFrames - 1) return prev;
        return prev + 1;
      });
    }, 600);
    return () => clearInterval(id);
  }, [points]);

  // Split saved points by actor
  const roverPoints = points.filter((p) => p.pointType === "rover");
  const userPoints = points.filter((p) => p.pointType === "user");
  const roverCoordinates = roverPoints.map((p) => ({ latitude: p.lat, longitude: p.lon }));
  const userCoordinates = userPoints.map((p) => ({ latitude: p.lat, longitude: p.lon }));
  // Playback markers advance together by index
  const roverMarker = roverPoints.length
    ? { latitude: roverPoints[Math.min(playIdx, roverPoints.length - 1)].lat, longitude: roverPoints[Math.min(playIdx, roverPoints.length - 1)].lon }
    : null;
  const userMarker = userPoints.length
    ? { latitude: userPoints[Math.min(playIdx, userPoints.length - 1)].lat, longitude: userPoints[Math.min(playIdx, userPoints.length - 1)].lon }
    : null;
  const centerPoint = roverMarker || userMarker;
  const initialRegion = centerPoint
    ? { ...centerPoint, latitudeDelta: 0.0015, longitudeDelta: 0.0015 }
    : { latitude: 30.6211667, longitude: -96.3403889, latitudeDelta: 0.0015, longitudeDelta: 0.0015 };

  return (
    <SafeAreaView style={styles.container}>
      <MapView style={styles.map} initialRegion={initialRegion}>
        {roverCoordinates.length > 1 ? (
          <Polyline coordinates={roverCoordinates} strokeColor="#e74c3c" strokeWidth={4} />
        ) : null}
        {userCoordinates.length > 1 ? (
          <Polyline coordinates={userCoordinates} strokeColor="#3498db" strokeWidth={4} />
        ) : null}
        {roverMarker ? (
          <Marker coordinate={roverMarker}>
            <Ionicons name="navigate-circle" size={28} color="#e74c3c" />
          </Marker>
        ) : null}
        {userMarker ? (
          <Marker coordinate={userMarker}>
            <Ionicons name="person-circle" size={24} color="#3498db" />
          </Marker>
        ) : null}
      </MapView>
      <View style={styles.infoBoxBottom}>
        <Text style={styles.infoTitle}>Playback</Text>
        <Text style={styles.infoText}>Session: {sessionId || "—"}</Text>
        <Text style={styles.infoText}>Rover Points: {roverPoints.length}</Text>
        <Text style={styles.infoText}>User Points: {userPoints.length}</Text>
      </View>
      <View style={styles.controlBar}>
        <TouchableOpacity
          style={[styles.button, { backgroundColor: "#7D3C98" }]}
          onPress={() => navigation.goBack()}
        >
          <Ionicons name="arrow-back" size={20} color="white" />
          <Text style={styles.buttonText}>Back</Text>
        </TouchableOpacity>
      </View>
    </SafeAreaView>
  );
}

// ----------------------------------------------------
// MAP SCREEN (YOUR ENTIRE APP HERE)
// ----------------------------------------------------
// Main map with tracking + controls
function MapScreen({ navigation, route }) {
  const { setUserPath, setRoverPath, pendingLogout, setPendingLogout } = React.useContext(ProfileContext);

  // Map refs + animation state
  const mapRef = useRef(null);
  const rotation = useRef(new Animated.Value(0)).current
  const [modeCooldown, setModeCooldown] = useState(false);
  const shownWelcomeRef = useRef(false);
  // Track previous ESP connection state for drop detection
  const prevEspConnectedRef = useRef(false);
  // Track if ESP connected at least once
  const hasSeenEspConnectionRef = useRef(false);
  const disconnectNotificationSentRef = useRef(false);
  // ESP32 connection state
  const [espConnected, setEspConnected] = useState(false);
  const [espLastSeen, setEspLastSeen] = useState(null);
  const [espError, setEspError] = useState("");
  const activeSessionIdRef = useRef(null);
  const sessionDistanceRef = useRef(0);
  const lastPointRef = useRef(null);
  const lastPointWriteMsRef = useRef(0);
  const lastUserPointRef = useRef(null);
  const lastUserPointWriteMsRef = useRef(0);
  const lastTelemetryWriteMsRef = useRef(0);
  // User location + smoothing
  const [userLocation, setUserLocation] = useState({
    latitude: 30.6211667,
    longitude: -96.3403889,
  });
  const userLocationAnim = useRef(new AnimatedRegion({userLocation})).current;
  const hasUserFix = useRef(false);
  const smoothedUserRef = useRef({userLocation});

  // Default initial region
  const zachRegion = {
    latitude: 30.6211667,
    longitude: -96.3403889,
    latitudeDelta: 0.0012,
    longitudeDelta: 0.0012,
  };

  // Rover data + mode
  const [roverData, setRoverData] = useState({
    latitude: 30.6211667,
    longitude: -96.3403889,
    // weight sensor state (default off)
    weight_on: false,
    mode: "STATE_IDLE",
  });
  // ESP32 base URL (SoftAP)
  const espBaseUrl = "http://192.168.4.1:80";
  // Convert ESP32 mode to UI label
  const mapEspMode = (mode) => {
    if (!mode) return null;
    if (mode === "autonomous") return "STATE_NAVIGATING";
    if (mode === "standby") return "STATE_IDLE";
    return mode;
  };
  // Ensure user profile exists in Firestore
  const ensureUserDoc = async () => {
    const user = auth.currentUser;
    if (!user) return;
    await setDoc(
      doc(db, "users", user.uid),
      {
        userId: user.uid,
        email: user.email || "",
        updatedAt: serverTimestamp(),
      },
      { merge: true }
    );
  };
  // Create a new active session
  const startSession = async (startMode) => {
    if (activeSessionIdRef.current) return;
    const user = auth.currentUser;
    if (!user) return;
    await ensureUserDoc();
    const sessionRef = await addDoc(collection(db, "sessions"), {
      userId: user.uid,
      startTime: serverTimestamp(),
      startMode: startMode,
      status: "active",
    });
    activeSessionIdRef.current = sessionRef.id;
    sessionDistanceRef.current = 0;
    lastPointRef.current = null;
    lastPointWriteMsRef.current = 0;
    lastUserPointRef.current = null;
    lastUserPointWriteMsRef.current = 0;
  };
  // Close the active session
  const finalizeSession = async (endMode, status = "complete") => {
    const user = auth.currentUser;
    const sessionId = activeSessionIdRef.current;
    if (!user || !sessionId) return;
    await updateDoc(doc(db, "sessions", sessionId), {
      userId: user.uid,
      endTime: serverTimestamp(),
      endMode: endMode,
      status: status,
      totalDistanceMeters: Number(sessionDistanceRef.current.toFixed(2)),
    });
    activeSessionIdRef.current = null;
    sessionDistanceRef.current = 0;
    lastPointRef.current = null;
    lastUserPointRef.current = null;
    lastPointWriteMsRef.current = 0;
    lastUserPointWriteMsRef.current = 0;
  };
  // Save one route point for playback
  const saveSessionPoint = async (lat, lon, pointType = "rover") => {
    const sessionId = activeSessionIdRef.current;
    if (!sessionId) return;
    const nowMs = Date.now();
    const currPoint = { latitude: lat, longitude: lon };
    const prevPoint = pointType === "rover" ? lastPointRef.current : lastUserPointRef.current;
    const movedMeters = prevPoint
      ? distanceMeters(prevPoint.latitude, prevPoint.longitude, currPoint.latitude, currPoint.longitude)
      : 0;
    const prevWriteMs = pointType === "rover" ? lastPointWriteMsRef.current : lastUserPointWriteMsRef.current;
    const dueByTime = nowMs - prevWriteMs >= 1000;
    const dueByMove = movedMeters >= 1.5;
    if (!dueByTime && !dueByMove) return;
    await addDoc(collection(db, "sessions", sessionId, "points"), {
      lat: lat,
      lon: lon,
      pointType: pointType,
      userId: auth.currentUser?.uid || null,
      clientTs: nowMs,
      ts: serverTimestamp(),
    });
    // Only rover movement contributes to rover distance
    if (pointType === "rover") {
      if (prevPoint) sessionDistanceRef.current += movedMeters;
      lastPointRef.current = currPoint;
      lastPointWriteMsRef.current = nowMs;
    } else {
      lastUserPointRef.current = currPoint;
      lastUserPointWriteMsRef.current = nowMs;
    }
  };
  // Save telemetry samples at slower rate
  const saveTelemetrySample = async (sample) => {
    const user = auth.currentUser;
    if (!user) return;
    const nowMs = Date.now();
    if (nowMs - lastTelemetryWriteMsRef.current < 10000) return;
    await addDoc(collection(db, "telemetry", user.uid, "samples"), {
      uid: user.uid,
      sessionId: activeSessionIdRef.current || null,
      weight_on: sample.weight_on ?? null,
      mode: sample.mode ?? null,
      ts: serverTimestamp(),
    });
    lastTelemetryWriteMsRef.current = nowMs;
  };

  // Heading + UI flags
  const [heading, setHeading] = useState(0);
  const [isUWBMode, setIsUWBMode] = useState(false);
  // Alarm state when weight goes from on -> off
  const [alarmActive, setAlarmActive] = useState(false);
  // Manual driving mode toggle
  const [isManualMode, setIsManualMode] = useState(false);
  // Current manual direction command
  const [manualDirection, setManualDirection] = useState(null);
  // Stop/Go state for manual movement animation
  const [manualGoEnabled, setManualGoEnabled] = useState(false);
  const [showAlert, setShowAlert] = useState(false);
  const alertAnim = useRef(new Animated.Value(-80)).current;

  // Show welcome popup once after login/signup
  useEffect(() => {
    if (!route?.params?.showWelcome) return;
    if (shownWelcomeRef.current) return;
    shownWelcomeRef.current = true;
    Alert.alert(
      "Welcome to Rover Tracker",
      "please connect to the ESP-32\n\nName: ESP32_APP\nPassword: esp32pass",
      [{ text: "OK" }]
    );
    // Clear the flag after the first popup
    navigation.setParams({ showWelcome: false });
  }, [route?.params?.showWelcome]);

  // Ask for local notification permission
  useEffect(() => {
    const setupNotifications = async () => {
      const perms = await Notifications.getPermissionsAsync();
      if (perms.status !== "granted") {
        await Notifications.requestPermissionsAsync();
      }
      await Notifications.setNotificationChannelAsync("esp-status", {
        name: "ESP Status",
        importance: Notifications.AndroidImportance.HIGH,
        sound: "default",
      });
    };
    setupNotifications().catch(() => {});
  }, []);

  // Send a local notification when ESP disconnects
  useEffect(() => {
    const wasConnected = prevEspConnectedRef.current;
    if (espConnected) {
      hasSeenEspConnectionRef.current = true;
    }
    if (
      hasSeenEspConnectionRef.current &&
      wasConnected &&
      !espConnected &&
      !disconnectNotificationSentRef.current
    ) {
      disconnectNotificationSentRef.current = true;
      Notifications.scheduleNotificationAsync({
        content: {
          title: "ESP-32 Disconnected",
          body: "Rover Tracker lost connection to the ESP-32.",
          sound: "default",
        },
        trigger: null,
      }).catch(() => {});
    }
    if (espConnected) {
      disconnectNotificationSentRef.current = false;
    }
    prevEspConnectedRef.current = espConnected;
  }, [espConnected]);

  // Keep rover in standby when ESP32 is disconnected
  useEffect(() => {
    if (!espConnected && roverData.mode !== "STATE_IDLE") {
      setRoverData((prev) => ({ ...prev, mode: "STATE_IDLE" }));
    }
  }, [espConnected, roverData.mode]);
  // Exit manual UI when ESP disconnects
  useEffect(() => {
    if (!espConnected && isManualMode) {
      setIsManualMode(false);
      setManualDirection(null);
      setManualGoEnabled(false);
    }
  }, [espConnected, isManualMode]);

  // Math helpers for distance
  // degrees -> radians
  const toRad = (deg) => (deg * Math.PI) / 180;
  // meters between two points
  const distanceMeters = (lat1, lon1, lat2, lon2) => {
    // Earth radius in meters
    const R = 6371e3;
    // Convert inputs and deltas to radians
    const φ1 = toRad(lat1);
    const φ2 = toRad(lat2);
    const dφ = toRad(lat2 - lat1);
    const dλ = toRad(lon2 - lon1);
    // Haversine step to get central angle
    const a =
      Math.sin(dφ / 2) ** 2 +
      Math.cos(φ1) * Math.cos(φ2) * Math.sin(dλ / 2) ** 2;
    // Convert central angle to distance
    return 2 * R * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  };
  // direction from user to rover
  const bearingToTarget = (lat1, lon1, lat2, lon2) => {
    // Convert inputs to radians
    const φ1 = toRad(lat1);
    const φ2 = toRad(lat2);
    const Δλ = toRad(lon2 - lon1);
    // Build x/y components for the direction
    const y = Math.sin(Δλ) * Math.cos(φ2);
    const x =
      Math.cos(φ1) * Math.sin(φ2) -
      Math.sin(φ1) * Math.cos(φ2) * Math.cos(Δλ);
    // Turn into degrees and wrap to 0-360
    const brng = (Math.atan2(y, x) * 180) / Math.PI;
    return (brng + 360) % 360;
  };


  // Cached distance for UI
  const distanceFeet = useMemo(
    () =>
      distanceMeters(
        userLocation.latitude,
        userLocation.longitude,
        roverData.latitude,
        roverData.longitude
      ) * 3.281,
    [userLocation, roverData]
  );

  // Add points only if we moved enough
  const appendPathPoint = (setPath, point, minMeters) => {
    setPath((prev) => {
      if (prev.length === 0) return [point];
      const last = prev[prev.length - 1];
      const d = distanceMeters(
        last.latitude,
        last.longitude,
        point.latitude,
        point.longitude
      );
      if (d < minMeters) return prev;
      return [...prev, point];
    });
  };

  // Track rover path for profile preview
  useEffect(() => {
    // Keep refs to location subscriptions so we can clean up
    let posSub = null;
    let headSub = null;

    (async () => {
      // Ask for foreground location permission
      const { status } = await Location.requestForegroundPermissionsAsync();
      if (status !== "granted") return;

      // Stream GPS updates with high accuracy
      posSub = await Location.watchPositionAsync(
        { accuracy: Location.Accuracy.BestForNavigation, timeInterval: 1, distanceInterval: 0 },
        (pos) => {
          const { latitude, longitude } = pos.coords;
          // snap to position and seed the path
          if (!hasUserFix.current) {
            userLocationAnim.setValue({ latitude, longitude });
            hasUserFix.current = true;
            smoothedUserRef.current = { latitude, longitude };
            appendPathPoint(setUserPath, { latitude, longitude }, 0);
          } else {
            // smooth the movement a bit
            const alpha = 0.5;
            const prev = smoothedUserRef.current;
            const smoothLat = prev.latitude + (latitude - prev.latitude) * alpha;
            const smoothLon = prev.longitude + (longitude - prev.longitude) * alpha;
            smoothedUserRef.current = { latitude: smoothLat, longitude: smoothLon };
            userLocationAnim.timing({
              latitude: smoothLat,
              longitude: smoothLon,
              duration: 50,
              useNativeDriver: false,
            }).start();
          }
          // Save current position and add to path
          setUserLocation({ latitude, longitude });
          appendPathPoint(setUserPath, { latitude, longitude }, 1.5);
        }
      );

      // Watch device heading for compass updates
      headSub = await Location.watchHeadingAsync((data) => {
        const { trueHeading } = data;
        // Ignore invalid readings
        if (!isFinite(trueHeading)) return;
        // Save heading and rotate the arrow smoothly
        setHeading(trueHeading);
        Animated.timing(rotation, {
          toValue: trueHeading,
          duration: 300,
          easing: Easing.linear,
          useNativeDriver: true,
        }).start();
      });
    })();

    // turn off trackers when exited
    return () => {
      posSub?.remove();
      headSub?.remove();
    };
  }, []);

  // ----------------------------------------------------
  // ESP32 Telemetry Polling (1 Hz)
  // ----------------------------------------------------
  useEffect(() => {
    let pollId = null;
    let isMounted = true;

    const pollTelemetry = async () => {
      let timeoutId = null;
      try {
        // Fetch telemetry from ESP32 with timeout
        const ctrl = new AbortController();
        timeoutId = setTimeout(() => ctrl.abort(), 1200);
        const res = await fetch(`${espBaseUrl}/telemetry`, { signal: ctrl.signal });
        clearTimeout(timeoutId);
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        // Parse JSON safely so one bad payload does not drop connection
        const raw = await res.text();
        let data = {};
        try {
          data = JSON.parse(raw);
        } catch {
          data = {};
        }

        if (!isMounted) return;
        // Mark ESP32 as connected
        setEspConnected(true);
        setEspLastSeen(Date.now());
        setEspError("");
        // Clean up the ESP event text
        const espEvent = String(data?.event || "").trim().toUpperCase();
        // Map ESP weight event to app weight state
        const getEventWeightState = () => {
          if (espEvent === "EVENT_WEIGHT_RESTORED") return true;
          if (espEvent === "EVENT_WEIGHT_REMOVED") return false;
          return null;
        };
        const eventWeightOn = getEventWeightState();
      
        // Using lat/lon from ESP for now
        setRoverData((prev) => {
          return {
            ...prev,
            latitude: data?.lat ?? data?.latitude ?? prev.latitude,
            longitude: data?.lon ?? data?.longitude ?? prev.longitude,
            // Show On/Off from the last ESP weight event
            weight_on: eventWeightOn ?? prev.weight_on,
            mode: mapEspMode(data?.mode) ?? prev.mode,
          };
        });
        // using lat/lon for now
      } catch (err) {
        if (!isMounted) return;
        // Mark ESP32 as disconnected on error
        setEspConnected(false);
        setEspError(err?.message || "Telemetry error");
      } finally {
        if (timeoutId) clearTimeout(timeoutId);
      }
    };

    // Poll once immediately, then every 1s
    pollTelemetry();
    pollId = setInterval(pollTelemetry, 1000);

    // Stop polling on exit
    return () => {
      isMounted = false;
      if (pollId) clearInterval(pollId);
    };
  }, []);

  useEffect(() => {
    // Save rover path as it moves
    appendPathPoint(
      setRoverPath,
      { latitude: roverData.latitude, longitude: roverData.longitude },
      1.0
    );
  }, [roverData.latitude, roverData.longitude]);
  // Save playback points while session is active
  useEffect(() => {
    if (!activeSessionIdRef.current) return;
    saveSessionPoint(roverData.latitude, roverData.longitude, "rover").catch(() => {});
  }, [roverData.latitude, roverData.longitude]);
  // Save user playback points while session is active
  useEffect(() => {
    if (!activeSessionIdRef.current) return;
    saveSessionPoint(userLocation.latitude, userLocation.longitude, "user").catch(() => {});
  }, [userLocation.latitude, userLocation.longitude]);
  // Start one session for the whole signed-in run
  useEffect(() => {
    startSession("Signed In").catch(() => {});
  }, []);
  // Finalize session on logout signal
  useEffect(() => {
    if (!pendingLogout) return;
    finalizeSession("STATE_IDLE", "complete")
      .catch(() => {})
      .finally(() => setPendingLogout(false));
  }, [pendingLogout]);


  // Mode change with a short cooldown
  const sendCommand = (cmd) => {
  if (modeCooldown) {
    Alert.alert("Please Wait", "You can only change modes every 1 second.");
    return;
  }

    setModeCooldown(true);
    // Optimistically update local mode so UI reflects the change immediately
    setRoverData((prev) => ({
      ...prev,
      mode: cmd === "Autonomous" ? "STATE_NAVIGATING" : cmd === "Standby" ? "STATE_IDLE" : "STATE_IDLE",
    }));
    // Session lifecycle is handled by sign-in/sign-out

    // Send command to ESP32
    const sendToEsp = async () => {
      try {
        // Map UI mode to ESP32 payload (use `state` key for ESP)
        const payload =
          cmd === "Autonomous" ? { state: "STATE_NAVIGATING" } :
          cmd === "Standby" ? { state: "STATE_IDLE" } :
          { state: "STATE_IDLE" };
        await fetch(`${espBaseUrl}/command`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(payload),
        });
      } catch (err) {
        Alert.alert("ESP32 Error", err?.message || "Command failed.");
      }
    };
    sendToEsp();

    // reset cooldown after 1 seconds
    setTimeout(() => setModeCooldown(false), 1000);
  };

  // Enter/exit manual mode and notify ESP32
  const toggleManualMode = async () => {
    const nextManualState = !isManualMode;
    try {
      const payload = nextManualState
        ? { state: "STATE_MANUAL_DRIVE" }
        : { state: "STATE_IDLE" };
      await fetch(`${espBaseUrl}/command`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      setIsManualMode(nextManualState);
      setRoverData((prev) => ({
        ...prev,
        mode: nextManualState ? "STATE_MANUAL_DRIVE" : "STATE_IDLE",
      }));
      // Reset local manual animation state when leaving manual mode
      if (!nextManualState) {
        setManualDirection(null);
        setManualGoEnabled(false);
      }
    } catch (err) {
      Alert.alert("ESP32 Error", err?.message || "Manual mode command failed.");
    }
  };

  // Send one manual control command
  const sendManualControl = async (control) => {
    try {
      await fetch(`${espBaseUrl}/command`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ state: "STATE_MANUAL_DRIVE", command: control }),
      });
      setRoverData((prev) => ({ ...prev, mode: "STATE_MANUAL_DRIVE" }));
      // Keep local manual animation state in sync with button press
      if (control === "stop") {
        setManualGoEnabled(false);
      } else if (control === "go") {
        setManualGoEnabled(true);
      } else {
        setManualDirection(control);
        setManualGoEnabled(true);
      }
    } catch (err) {
      Alert.alert("ESP32 Error", err?.message || "Manual control failed.");
    }
  };

  // Send alarm-off command to ESP32 and clear local alarm state
  const sendAlarmOff = async () => {
    try {
      // Send the alarm-off payload to ESP32
      const payload = { alarm: "off" };
      await fetch(`${espBaseUrl}/command`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      setAlarmActive(false);
      Alert.alert("Alarm", "Sent ALARM_OFF to rover.");
    } catch (err) {
      Alert.alert("ESP32 Error", err?.message || "Alarm command failed.");
    }
  };

  // Recenter map to fit user + rover
  const recenterBoth = () => {
    const u = userLocation, r = roverData;

    // Calculate midpoint between user and rover
    const midLat = (u.latitude + r.latitude) / 2;
    const midLng = (u.longitude + r.longitude) / 2;

    // Calculate distance (in meters)
    const dist = distanceMeters(u.latitude, u.longitude, r.latitude, r.longitude);

    // Adjust zoom based on distance — closer = tighter zoom
    let zoomFactor = 0.0025;
    if (dist > 15) zoomFactor = 0.001;
    if (dist > 30) zoomFactor = 0.002;
    if (dist > 60) zoomFactor = 0.0025;

    mapRef.current?.animateToRegion(
      {
        latitude: midLat,
        longitude: midLng,
        latitudeDelta: zoomFactor,
        longitudeDelta: zoomFactor,
      },
      800 // smooth transition (ms)
    );
  };

  useEffect(() => {
  // Automatically recenter map when this screen is opened
    const timer = setTimeout(() => {
      if (mapRef.current) recenterBoth();
    }, 600); // wait ~0.6s for map and markers to load

    return () => clearTimeout(timer);
  }, []);

  // ----------------------------------------------------
  // Reliable 6 ft Alert Banner (In-App Only)
  // ----------------------------------------------------
  useEffect(() => {
    // Skip if we don't have both positions yet
    if (!userLocation || !roverData) return;

    const checkDistance = () => {
      // Measure distance between user and rover
      const distMeters = distanceMeters(
        userLocation.latitude,
        userLocation.longitude,
        roverData.latitude,
        roverData.longitude
      );
      const distFeet = distMeters * 3.281;

      if (roverData.mode === "STATE_NAVIGATING") {
        // Show banner if rover drifts out of range
        if (distFeet > 6 && !showAlert) {
          setShowAlert(true);
          Animated.timing(alertAnim, {
            toValue: 0,
            duration: 400,
            easing: Easing.out(Easing.ease),
            useNativeDriver: true,
          }).start();
        // Hide banner once rover is back in range
        } else if (distFeet <= 6 && showAlert) {
          setShowAlert(false);
          Animated.timing(alertAnim, {
            toValue: -80,
            duration: 400,
            easing: Easing.in(Easing.ease),
            useNativeDriver: true,
          }).start();
        }
      } else if (showAlert) {
        // Hide banner when not in Autonomous mode
        setShowAlert(false);
        Animated.timing(alertAnim, {
          toValue: -80,
          duration: 300,
          useNativeDriver: true,
        }).start();
      }
    };

    // Run once now, then on an interval
    checkDistance();
    const interval = setInterval(checkDistance, .001);

    // Clear the interval on exit
    return () => clearInterval(interval);
  }, [
    roverData.mode,
    roverData.latitude,
    roverData.longitude,
    userLocation,
    showAlert,
    alertAnim,
  ]);

  // UWB Screen 

  // Show UWB view instead of map
  if (isUWBMode) {
    // Angle from user to rover, adjusted by phone heading
    const brng = (bearingToTarget(
      userLocation.latitude,
      userLocation.longitude,
      roverData.latitude,
      roverData.longitude
    ) - heading + 360) % 360;

    // Convert to radians for rotation transform
    const arrowRotation = brng * (Math.PI / 180);

    return (
      <SafeAreaView style={styles.uwbContainer}>
        <Text style={styles.uwbTitle}>UWB Navigation Mode</Text>
        <View style={{ alignItems: "center", marginVertical: 80 }}>
          {/* Rotating arrow points toward the rover */}
          <Animated.View style={{ transform: [{ rotate: `${arrowRotation}rad` }] }}>
            <View style={styles.arrowCircle}>
              <Ionicons name="arrow-up" size={64} color="white" />
            </View>
          </Animated.View>
          <Text style={styles.uwbDistance}>
            Distance: {distanceFeet.toFixed(1)} ft
          </Text>
        </View>
        <TouchableOpacity
          style={styles.exitButton}
          onPress={() => {
            // Exit UWB mode and recenter the map
            setIsUWBMode(false);
            setTimeout(recenterBoth, 600); // smooth recenter after leaving UWB
          }}
        >
          <Ionicons name="close-circle" size={20} color="white" />
          <Text style={styles.exitText}>Exit Navigation</Text>
        </TouchableOpacity>
      </SafeAreaView>
    );
  }

  return (
    
    <SafeAreaView style={styles.container}>
      {/* MAP + UI */}
      <MapView
        ref={mapRef}
        style={styles.map}
        showsUserLocation={false}
        initialRegion={zachRegion}
      >
       
        {/* ---- Profile Button (Top Right) ---- */}
        <View style={styles.profileButtonContainer}>
          <TouchableOpacity
            style={styles.profileButton}
            onPress={() => navigation.navigate("Profile")}
          >
            <Ionicons name="person-circle-outline" size={26} color="white" />
          </TouchableOpacity>
        </View>

        {/* Rover marker */}
        <Marker
          coordinate={roverData}
          title="Rover"
          tracksViewChanges={false}
          stopPropagation={true}
        >
          <View
            style={{
              backgroundColor: "#500000",
              borderRadius: 20,
              padding: 6,
              shadowColor: "#000",
              shadowOpacity: 0.25,
              shadowRadius: 3,
              elevation: 6,
            }}
          >
            <MaterialCommunityIcons name="robot" size={28} color="white" />
          </View>
        </Marker>

        {/* User marker (animated) */}
        <Marker.Animated
          coordinate={userLocationAnim}
          anchor={{ x: 0.5, y: 0.5 }}
        >
          <View
            style={{
              width: 18,
              height: 18,
              backgroundColor: "#007bff",
              borderRadius: 9,
              borderWidth: 2,
              borderColor: "white",
            }}
          />
        </Marker.Animated>
        {/* Home marker removed (Go Home feature disabled) */}
      </MapView>

      {/* Recenter button */}
      <View style={styles.recenterContainer}>
        <TouchableOpacity style={styles.recenterButton} onPress={recenterBoth}>
          <Ionicons name="locate" size={22} color="white" />
        </TouchableOpacity>
      </View>

      {/* Rover status card */}
      <View style={styles.infoBoxBottom}>
        <View style={styles.infoRow}>
          <MaterialCommunityIcons name="robot" size={20} color="#007bff" />
          <Text style={styles.infoTitle}> Rover Status</Text>
        </View>
        <Text style={styles.infoText}>Mode: {roverData.mode}</Text>
        <Text style={styles.infoText}>Weight: {roverData.weight_on ? "On" : "Off"}</Text>
        <Text style={styles.infoText}>Distance: {distanceFeet.toFixed(1)} ft</Text>
        {/* ESP32 status */}
        <Text style={styles.infoText}>ESP32: {espConnected ? "Connected" : "Disconnected"}</Text>
        {espError ? (
          <Text style={styles.infoText}>ESP Error: {espError}</Text>
        ) : null}
        {alarmActive ? (
          <View style={[styles.alertInBox, { backgroundColor: "#c0392b", marginTop: 8 }]}>
            <Text style={styles.alertInBoxText}>⚠️ Item Removed — Alarm Active</Text>
          </View>
        ) : null}
        <Text style={styles.infoText}>
          {roverData.mode === "STATE_NAVIGATING" && distanceFeet > 6 && (
            <View style={styles.alertInBox}>
              <Text style={styles.alertInBoxText}>⚠️ Rover Out of Range (6 ft)</Text>
            </View>
          )}
        </Text>

      </View>

      {/* Mode buttons */}
      <View style={styles.controlBar}>
        <TouchableOpacity
          style={[styles.button, { backgroundColor: "#2ecc71" }]}
          onPress={() => sendCommand("Autonomous")}
        >
          <Ionicons name="navigate" size={20} color="white" />
          <Text style={styles.buttonText}>Autonomous</Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={[styles.button, { backgroundColor: "#e74c3c" }]}
          onPress={() => sendCommand("Standby")}
        >
          <Ionicons name="pause-circle" size={20} color="white" />
          <Text style={styles.buttonText}>Standby</Text>
        </TouchableOpacity>

        {/* Manual mode toggle button */}
        <TouchableOpacity
          style={[styles.button, { backgroundColor: isManualMode ? "#1f7a47" : "#f39c12" }]}
          onPress={toggleManualMode}
        >
          <Ionicons name="game-controller" size={20} color="white" />
          <Text style={styles.buttonText}>{isManualMode ? "Exit Manual" : "Manual Mode"}</Text>
        </TouchableOpacity>

        
      </View>

      {/* Navigation + Return Home buttons */}
      <View style={styles.directionsContainer}>
        <TouchableOpacity
          style={[styles.directionsButton, { backgroundColor: "#7D3C98", marginTop: 10, flex: 0.45 }]}
          onPress={() => setIsUWBMode(true)}
        >
          <Ionicons name="compass" size={20} color="white" />
          <Text style={styles.buttonText}>Navigation Mode</Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={[styles.directionsButton, { backgroundColor: "#34495e", marginTop: 10, flex: 0.45 }]}
          onPress={sendAlarmOff}
        >
          <Ionicons name="notifications-off" size={20} color="white" />
          <Text style={styles.buttonText}>Alarm Off</Text>
        </TouchableOpacity>
        {/* Go Home button removed */}
      </View>

      {/* Manual controls (sent to ESP32) */}
      {isManualMode ? (
        <View style={styles.manualPadContainer}>
          <TouchableOpacity
            style={styles.manualArrowButton} 
            onLongPress={() => sendManualControl("forward")}
            onPressOut={() => sendManualControl("stop")}
          >
            {/* Forward */}
            <Ionicons name="arrow-up" size={24} color="white" />
          </TouchableOpacity>

          {/* Top-corner turn buttons removed */}

          <View style={styles.manualMidRow}>
            <TouchableOpacity
              style={styles.manualArrowButton}
              onLongPress={() => sendManualControl("turnleft")}
              onPressOut={() => sendManualControl("stop")}
            >
              {/* Turn Left */}
              <Ionicons name="arrow-back" size={24} color="white" />
            </TouchableOpacity>

            <View style={styles.manualCenterButtons}>
              <TouchableOpacity
                style={[styles.manualCenterButton, { backgroundColor: "#c0392b" }]}
                onPress={() => sendManualControl("stop")}
              >
                {/* Stop */}
                <Text style={styles.manualCenterText}>Stop</Text>
              </TouchableOpacity>
              <TouchableOpacity
                style={[styles.manualCenterButton, { backgroundColor: "#27ae60" }]}
                onPress={() => sendManualControl("go")}
              >
                {/* Go */}
                <Text style={styles.manualCenterText}>Go</Text>
              </TouchableOpacity>
            </View>
            
            <TouchableOpacity
              style={styles.manualArrowButton}
              onLongPress={() => sendManualControl("turnright")}
              onPressOut={() => sendManualControl("stop")}
            >
              {/* Turn Right */}
              <Ionicons name="arrow-forward" size={24} color="white" />
            </TouchableOpacity>
          </View>

          <TouchableOpacity
            style={styles.manualArrowButton}
            onLongPress={() => sendManualControl("reverse")}
            onPressOut={() => sendManualControl("stop")}
          >
            {/* Reverse */}
            <Ionicons name="arrow-down" size={24} color="white" />
          </TouchableOpacity>

          {/* Reverse-left and reverse-right removed */}

        </View>
      ) : null}
    </SafeAreaView>
  );
}

// ----------------------------------------------------
// FINAL APP NAVIGATION WRAPPER
// ----------------------------------------------------
export default function App() {
  const [profile, setProfile] = useState({
    name: "Yazan Chtay",
    email: "GoatJames@tamu.edu",
    lastLogin: new Date().toLocaleString()
  });
  const [userPath, setUserPath] = useState([]);
  const [roverPath, setRoverPath] = useState([]);
  const [pendingLogout, setPendingLogout] = useState(false);

  return (
    <ProfileContext.Provider value={{ profile, setProfile, userPath, roverPath, setUserPath, setRoverPath, pendingLogout, setPendingLogout }}>
      <NavigationContainer>
        <Stack.Navigator screenOptions={{ headerShown: false }}>
          <Stack.Screen name="Login" component={LoginScreen} />
          <Stack.Screen name="SignUp" component={SignUpScreen} />
          <Stack.Screen name="Map" component={MapScreen} />
          <Stack.Screen name="Profile" component={ProfileScreen} />
          <Stack.Screen name="EditProfile" component={EditProfileScreen} />
          <Stack.Screen name="History" component={HistoryScreen} />
          <Stack.Screen name="Playback" component={PlaybackScreen} />
        </Stack.Navigator>
      </NavigationContainer>
    </ProfileContext.Provider>
  );
}

// ----------------------------------------------------
// STYLES (Login + Map styles combined)
// ----------------------------------------------------
const styles = StyleSheet.create({
  // Login screen styles
  loginContainer: {
    flex: 1,
    backgroundColor: "#001f3f",
    alignItems: "center",
    justifyContent: "center",
  },
  loginTitle: {
    fontSize: 32,
    color: "white",
    fontWeight: "900",
    marginTop: 10,
    marginBottom: 30,
  },
  loginBox: {
    width: "85%",
    backgroundColor: "rgba(255,255,255,0.12)",
    padding: 20,
    borderRadius: 14,
  },
  loginLabel: { color: "white", fontSize: 14, marginTop: 12 },
  inputBox: {
    padding: 14,
    backgroundColor: "rgba(255,255,255,0.25)",
    borderRadius: 10,
    marginTop: 5,
  },
  loginInput: {
    color: "white",
    fontSize: 16,
  },
  loginButton: {
    backgroundColor: "#7D3C98",
    padding: 14,
    alignItems: "center",
    borderRadius: 10,
    marginTop: 25,
  },
  // Dim disabled auth buttons
  disabledButton: {
    opacity: 0.6,
  },
  loginButtonText: { color: "white", fontSize: 18, fontWeight: "700" },
  // Sign up link styles
  signupLink: {
    marginTop: 14,
    alignItems: "center",
  },
  signupLinkText: {
    color: "#cfe6ff",
    fontSize: 14,
    fontWeight: "600",
  },

  // Map screen styles
  container: { flex: 1, backgroundColor: "#001f3f" },
  map: { flex: 1 },
  infoBoxBottom: {
    position: "absolute",
    bottom: 190,
    left: 20,
    right: 20,
    backgroundColor: "rgba(255,255,255,0.95)",
    padding: 15,
    borderRadius: 14,
  },
  infoRow: { flexDirection: "row", alignItems: "center", marginBottom: 6 },
  infoTitle: { fontWeight: "bold", fontSize: 16, color: "#001f3f" },
  infoText: { fontSize: 14, color: "#333" },
  controlBar: {
    position: "absolute",
    bottom: 110,
    flexDirection: "row",
    justifyContent: "space-evenly",
    width: "100%",
    paddingHorizontal: 20,
  },
  button: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "center",
    flex: 1,
    marginHorizontal: 2,
    paddingVertical: 14,
    borderRadius: 12,
  },
  buttonText: { color: "white", fontWeight: "600", fontSize: 16, marginLeft: 6 },
  directionsContainer: {
  position: "absolute",
  bottom: 40,
  width: "100%",
  flexDirection: "row",
  justifyContent: "space-evenly",
  alignItems: "center",
  paddingHorizontal: 20,
  },
  directionsButton: {
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "center",
    paddingVertical: 14,
    borderRadius: 12,
    shadowColor: "#000",
    shadowOpacity: 0.2,
    shadowRadius: 4,
  },
  // Manual control pad styles
  manualPadContainer: {
    position: "absolute",
    bottom: 190,
    alignSelf: "center",
    alignItems: "center",
    backgroundColor: "rgba(0,0,0,0.35)",
    padding: 12,
    borderRadius: 14,
  },
  manualMidRow: {
    flexDirection: "row",
    alignItems: "center",
    marginVertical: 8,
  },
  manualArrowButton: {
    width: 54,
    height: 54,
    borderRadius: 27,
    backgroundColor: "#2c3e50",
    alignItems: "center",
    justifyContent: "center",
    marginHorizontal: 10,
  },

  manualArrowButtonTopLeft: {
    width: 54,
    height: 54,
    borderRadius: 27,
    backgroundColor: "#2c3e50",
    alignItems: "center",
    justifyContent: "center",
    marginHorizontal: 10,
    position: "absolute",
    top: 10,
    left: 10,
  },

  manualArrowButtonTopRight: {
    width: 54,
    height: 54,
    borderRadius: 27,
    backgroundColor: "#2c3e50",
    alignItems: "center",
    justifyContent: "center",
    marginHorizontal: 10,
    position: "absolute",
    top: 10,
    right: 10,
  },

  manualArrowButtonBottomLeft: {
    width: 54,
    height: 54,
    borderRadius: 27,
    backgroundColor: "#2c3e50",
    alignItems: "center",
    justifyContent: "center",
    marginHorizontal: 10,
    position: "absolute",
    bottom: 10,
    left: 10,
  },

  manualArrowButtonBottomright: {
    width: 54,
    height: 54,
    borderRadius: 27,
    backgroundColor: "#2c3e50",
    alignItems: "center",
    justifyContent: "center",
    marginHorizontal: 10,
    position: "absolute",
    bottom: 10,
    right: 10,
  },

  manualCenterButtons: {
    alignItems: "center",
    justifyContent: "center",
    marginHorizontal: 8,
  },
  manualCenterButton: {
    minWidth: 72,
    paddingVertical: 8,
    paddingHorizontal: 10,
    borderRadius: 10,
    alignItems: "center",
    marginVertical: 4,
  },
  manualCenterText: {
    color: "white",
    fontWeight: "700",
    fontSize: 14,
  },

  // Recenter button styles
  recenterContainer: { position: "absolute", top: 80, left: 20 },
  recenterButton: {
    backgroundColor: "#007bff",
    padding: 12,
    borderRadius: 30,
  },
  // UWB screen styles
  uwbContainer: {
    flex: 1,
    backgroundColor: "#001f3f",
    alignItems: "center",
    justifyContent: "center",
  },
  uwbTitle: {
    fontSize: 22,
    color: "white",
    fontWeight: "700",
    marginBottom: 20,
  },
  arrowCircle: {
    backgroundColor: "#007bff",
    width: 120,
    height: 120,
    borderRadius: 60,
    alignItems: "center",
    justifyContent: "center",
  },
  uwbDistance: {
    color: "white",
    fontSize: 18,
    marginTop: 40,
    fontWeight: "500",
  },
  exitButton: {
    backgroundColor: "#e74c3c",
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "center",
    paddingVertical: 16,
    paddingHorizontal: 80,
    borderRadius: 12,
  },
  exitText: {
    color: "white",
    fontSize: 18,
    fontWeight: "600",
    marginLeft: 8,
  },
  // Info box styling (map screen)
  infoBoxBottom: {
    position: "absolute",
    bottom: 190,
    left: 10,
    right: 10,
    backgroundColor: "rgba(255,255,255,0.95)",
    padding: 25,
    borderRadius: 14,
    shadowColor: "#000",
    shadowOpacity: 0.2,
    shadowRadius: 6,
    elevation: 6,
    flexShrink: 0,    // prevents shrinking
    flexGrow: 1,      // allows it to expand
    alignSelf: "stretch",
  },

  // Alert text inside the info box
  alertInBoxText: {
    color: "#000",
    fontWeight: "700",
    fontSize: 14,
  },

  // Profile button styles
  profileButtonContainer: {
    position: "absolute",
    top: 20,
    right: 20,
    zIndex: 999,
  },

  profileButton: {
    backgroundColor: "rgba(255,255,255,0.2)",
    padding: 10,
    borderRadius: 30,
    shadowColor: "#000",
    shadowOpacity: 0.2,
    shadowRadius: 5,
    elevation: 6,
  },

  // Profile screen styles
  profileContainer: {
    flex: 1,
    backgroundColor: "#001f3f",
    alignItems: "center",
    paddingTop: 40,
  },

  profileCardFixed: {
    width: "88%",
    backgroundColor: "#203a5c",
    padding: 25,
    borderRadius: 20,
    marginTop: 40,
  },

  profileMapContainer: {
    width: "88%",
    height: 180,
    borderRadius: 16,
    overflow: "hidden",
    marginTop: 20,
  },

  profileMap: {
    flex: 1,
  },

  profileLabelFixed: {
    color: "#d0d0d0",
    fontSize: 15,
    marginBottom: 2,
  },

  profileValueFixed: {
    color: "white",
    fontSize: 20,
    fontWeight: "700",
    marginBottom: 14,
  },

  profileBackButtonFixed: {
    flexDirection: "row",
    alignItems: "center",
    backgroundColor: "#7D3C98",
    paddingVertical: 14,
    paddingHorizontal: 60,
    borderRadius: 12,
    marginTop: 40,
  },
  // History button on profile
  profileHistoryButton: {
    flexDirection: "row",
    alignItems: "center",
    backgroundColor: "#2f7aa8",
    paddingVertical: 14,
    paddingHorizontal: 46,
    borderRadius: 12,
    marginTop: 14,
  },

  profileBackTextFixed: {
    color: "white",
    fontSize: 18,
    fontWeight: "600",
    marginLeft: 8,
  },

  profileSignOutButtonFixed: {
    position: "absolute",
    bottom: 35,
    right: 25,
    backgroundColor: "#e74c3c",
    flexDirection: "row",
    alignItems: "center",
    paddingVertical: 10,
    paddingHorizontal: 20,
    borderRadius: 12,
    shadowColor: "#000",
    shadowOpacity: 0.3,
    shadowRadius: 4,
    elevation: 6,
  },

  profileSignOutTextFixed: {
    color: "white",
    fontSize: 16,
    fontWeight: "600",
    marginLeft: 8,
  },
  // History list container
  historyList: {
    width: "88%",
    maxHeight: 500,
    backgroundColor: "#203a5c",
    borderRadius: 16,
    padding: 14,
  },
  // Scroll area for history rows
  historyScroll: {
    width: "100%",
  },
  historyListContent: {
    paddingBottom: 8,
  },
  // One history row card
  historyRow: {
    backgroundColor: "#2c4a70",
    borderRadius: 12,
    padding: 12,
    marginBottom: 10,
  },
  historyTitle: {
    color: "white",
    fontSize: 16,
    fontWeight: "700",
    marginBottom: 4,
  },
  historyText: {
    color: "#d9e6f2",
    fontSize: 13,
    marginBottom: 2,
  },

  // Edit profile button styles
  editProfileButtonFixed: {
    flexDirection: "row",
    backgroundColor: "#7D3C98",
    paddingVertical: 12,
    paddingHorizontal: 35,
    borderRadius: 12,
    alignItems: "center",
    justifyContent: "center",
    marginTop: 20,
  },

  editProfileTextFixed: {
    color: "white",
    fontSize: 18,
    fontWeight: "600",
    marginLeft: 8,
  },


  // Edit profile screen styles
  editContainer: {
    flex: 1,
    backgroundColor: "#001f3f",
    alignItems: "center",
    paddingTop: 50,
  },

  editTitle: {
    fontSize: 28,
    color: "white",
    fontWeight: "800",
    marginBottom: 30,
  },

  editBox: {
    width: "85%",
    backgroundColor: "rgba(255,255,255,0.12)",
    padding: 20,
    borderRadius: 14,
  },

  editLabel: {
    color: "white",
    fontSize: 14,
    marginTop: 12,
    marginBottom: 4,
  },

  editInputBox: {
    padding: 14,
    backgroundColor: "rgba(255,255,255,0.25)",
    borderRadius: 10,
    marginBottom: 12,
    color: "white",
    fontSize: 16,
  },

  editInput: {
    color: "white",
    fontSize: 18,
    fontWeight: "600",
  },

  // Save button styles
  saveButton: {
    backgroundColor: "#2ecc71",
    paddingVertical: 14,
    paddingHorizontal: 70,
    borderRadius: 12,
    marginTop: 40,
  },

  saveButtonText: {
    color: "white",
    fontSize: 18,
    fontWeight: "700",
  },

});
