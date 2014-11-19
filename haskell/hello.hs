import Data.Text
import qualified Data.Text.IO as TIO
import qualified Data.Map as Map
import Control.Applicative

{- findkey :: (Eq k) => k -> [(k,v)] -> v -}
{- findkey key xs = snd.head.filter (\(k,v) -> key == k) $ xs -}

class YesNo a where
    yn :: a -> Bool

instance YesNo Int where
    yn 2 = True
    yn _ = False

showYesNo :: Int -> Bool
showYesNo a = yn a


data LoginError = InvalidEmail deriving Show

getDomain :: Text -> Either LoginError Text
getDomain email = case splitOn (pack "@") email of
    [name, domain] -> Right domain
    _              -> Left InvalidEmail

-- io action: every io action that gets performed has a result that encapsulated within it.
printResult :: Either LoginError Text -> IO ()
printResult domain = case domain of
    Right text -> TIO.putStrLn(append (pack "Domain:") text)
    Left InvalidEmail -> TIO.putStrLn (pack "Error: invalid domain")

-- io action
getToken :: IO (Either LoginError Text)
getToken = do
    TIO.putStrLn (pack "Enter email addr:")
    email <- TIO.getLine
    return (getDomain email)

-- main --
main = do
putStrLn "hello, world\n"
a <- return "hell"
b <- return "yeah"
putStrLn (a ++ " " ++ b ++ "\n")
getToken
{- putStrLn "hello again, please input your email:\n" -}
{- name <- getLine -}
{- putStrLn ("get your email:" ++ name) -}

{- printResult (getDomain (pack name)) -}
