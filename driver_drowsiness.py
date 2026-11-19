import cv2
import mediapipe as mp
import numpy as np
import requests
import time


# ============================================================
# TELEGRAM CONFIGURATION
# ============================================================

BOT_TOKEN = "8825431037:AAHf5zqfv8WRz4xjfjWxpQW37GPLA20DuUQ"
CHAT_ID = "6299100891"


# ============================================================
# DROWSINESS CONFIGURATION
# ============================================================

# Adjust this value after testing
EAR_THRESHOLD = 0.21

# Eye must remain closed for this many seconds
CLOSED_TIME_THRESHOLD = 5.0

# Time between Telegram alerts
ALERT_COOLDOWN = 30


# ============================================================
# TELEGRAM FUNCTION
# ============================================================

def send_telegram(message):

    url = f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage"

    data = {
        "chat_id": CHAT_ID,
        "text": message
    }

    try:

        response = requests.post(
            url,
            data=data,
            timeout=10
        )

        if response.status_code == 200:
            print("Telegram notification sent")

        else:
            print("Telegram error:")
            print(response.text)

    except Exception as e:

        print("Telegram connection error:")
        print(e)


# ============================================================
# MEDIAPIPE
# ============================================================

mp_face_mesh = mp.solutions.face_mesh

face_mesh = mp_face_mesh.FaceMesh(
    static_image_mode=False,
    max_num_faces=1,
    refine_landmarks=True,
    min_detection_confidence=0.5,
    min_tracking_confidence=0.5
)


# ============================================================
# EYE LANDMARKS
# ============================================================

# MediaPipe landmark indices
# Left eye
LEFT_EYE = [
    362,
    385,
    387,
    263,
    373,
    380
]

# Right eye
RIGHT_EYE = [
    33,
    160,
    158,
    133,
    153,
    144
]


# ============================================================
# EAR FUNCTION
# ============================================================

def calculate_ear(eye_points):

    # Vertical distance 1
    vertical_1 = np.linalg.norm(
        eye_points[1] - eye_points[5]
    )

    # Vertical distance 2
    vertical_2 = np.linalg.norm(
        eye_points[2] - eye_points[4]
    )

    # Horizontal distance
    horizontal = np.linalg.norm(
        eye_points[0] - eye_points[3]
    )

    if horizontal == 0:
        return 0

    ear = (
        vertical_1 + vertical_2
    ) / (2.0 * horizontal)

    return ear


# ============================================================
# CAMERA
# ============================================================

cap = cv2.VideoCapture(0)

if not cap.isOpened():

    print("ERROR: Webcam not detected")

    exit()


# Optional camera resolution

cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)


# ============================================================
# VARIABLES
# ============================================================

eyes_closed = False

closed_start_time = None

last_alert_time = 0

alert_sent = False


print("-----------------------------------------")
print(" DRIVER DROWSINESS MONITORING SYSTEM")
print("-----------------------------------------")
print("Camera started")
print("Press Q to exit")
print("-----------------------------------------")


# ============================================================
# MAIN LOOP
# ============================================================

while True:

    ret, frame = cap.read()

    if not ret:

        print("Unable to read camera frame")

        break


    # --------------------------------------------------------
    # Mirror image
    # --------------------------------------------------------

    frame = cv2.flip(frame, 1)


    # --------------------------------------------------------
    # Convert BGR to RGB
    # --------------------------------------------------------

    rgb_frame = cv2.cvtColor(
        frame,
        cv2.COLOR_BGR2RGB
    )


    # --------------------------------------------------------
    # Process face
    # --------------------------------------------------------

    results = face_mesh.process(rgb_frame)


    status = "NO FACE"


    # --------------------------------------------------------
    # Face detected
    # --------------------------------------------------------

    if results.multi_face_landmarks:

        face_landmarks = results.multi_face_landmarks[0]


        h, w, _ = frame.shape


        # ----------------------------------------------------
        # Get eye coordinates
        # ----------------------------------------------------

        left_points = []

        right_points = []


        for index in LEFT_EYE:

            landmark = face_landmarks.landmark[index]

            x = int(landmark.x * w)
            y = int(landmark.y * h)

            left_points.append([x, y])


        for index in RIGHT_EYE:

            landmark = face_landmarks.landmark[index]

            x = int(landmark.x * w)
            y = int(landmark.y * h)

            right_points.append([x, y])


        left_points = np.array(
            left_points,
            dtype=np.float32
        )

        right_points = np.array(
            right_points,
            dtype=np.float32
        )


        # ----------------------------------------------------
        # Calculate EAR
        # ----------------------------------------------------

        left_ear = calculate_ear(
            left_points
        )

        right_ear = calculate_ear(
            right_points
        )


        average_ear = (
            left_ear + right_ear
        ) / 2.0


        # ----------------------------------------------------
        # Determine eye status
        # ----------------------------------------------------

        if average_ear < EAR_THRESHOLD:

            status = "EYES CLOSED"

            if not eyes_closed:

                eyes_closed = True

                closed_start_time = time.time()

                alert_sent = False


            # How long eyes have been closed

            closed_duration = (
                time.time() - closed_start_time
            )


            # ------------------------------------------------
            # DROWSINESS DETECTED
            # ------------------------------------------------

            if (
                closed_duration >= CLOSED_TIME_THRESHOLD
                and not alert_sent
            ):

                current_time = time.time()


                # Telegram cooldown

                if (
                    current_time - last_alert_time
                    >= ALERT_COOLDOWN
                ):

                    message = (
                        "🚨 DRIVER DROWSINESS ALERT 🚨\n\n"
                        "The driver's eyes have remained "
                        "closed for an extended period.\n\n"
                        "⚠️ Please check the driver immediately."
                    )

                    send_telegram(message)

                    last_alert_time = current_time

                    alert_sent = True


            # Display closed duration

            cv2.putText(
                frame,
                f"Closed: {closed_duration:.1f}s",
                (20, 90),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.8,
                (0, 0, 255),
                2
            )


        else:

            status = "EYES OPEN"

            eyes_closed = False

            closed_start_time = None

            alert_sent = False


        # ----------------------------------------------------
        # Draw eye landmarks
        # ----------------------------------------------------

        for point in left_points:

            x, y = point.astype(int)

            cv2.circle(
                frame,
                (x, y),
                2,
                (255, 255, 0),
                -1
            )


        for point in right_points:

            x, y = point.astype(int)

            cv2.circle(
                frame,
                (x, y),
                2,
                (255, 255, 0),
                -1
            )


        # ----------------------------------------------------
        # Display EAR
        # ----------------------------------------------------

        cv2.putText(
            frame,
            f"EAR: {average_ear:.3f}",
            (20, 30),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.7,
            (255, 255, 255),
            2
        )


        # ----------------------------------------------------
        # Display status
        # ----------------------------------------------------

        cv2.putText(
            frame,
            status,
            (20, 60),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.8,
            (255, 255, 255),
            2
        )


    else:

        status = "NO FACE"

        cv2.putText(
            frame,
            "NO FACE DETECTED",
            (20, 60),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.8,
            (0, 255, 255),
            2
        )


        eyes_closed = False

        closed_start_time = None

        alert_sent = False


    # ========================================================
    # DISPLAY
    # ========================================================

    cv2.imshow(
        "Driver Drowsiness Monitoring",
        frame
    )


    # ========================================================
    # EXIT
    # ========================================================

    key = cv2.waitKey(1) & 0xFF

    if key == ord("q"):

        break


# ============================================================
# CLEANUP
# ============================================================

cap.release()

cv2.destroyAllWindows()

face_mesh.close()

print("Driver monitoring stopped.")